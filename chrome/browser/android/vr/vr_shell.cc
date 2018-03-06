// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell.h"

#include <android/native_window_jni.h>

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/android_ui_gesture_target.h"
#include "chrome/browser/android/vr/autocomplete_controller.h"
#include "chrome/browser/android/vr/vr_gl_thread.h"
#include "chrome/browser/android/vr/vr_input_connection.h"
#include "chrome/browser/android/vr/vr_shell_delegate.h"
#include "chrome/browser/android/vr/vr_shell_gl.h"
#include "chrome/browser/android/vr/vr_usage_monitor.h"
#include "chrome/browser/android/vr/vr_web_contents_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/metrics_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/toolbar_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "device/vr/android/gvr/cardboard_gamepad_data_fetcher.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "device/vr/vr_device.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "jni/VrShellImpl_jni.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/android/surface_texture.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace vr {

namespace {
vr::VrShell* g_vr_shell_instance;

constexpr base::TimeDelta poll_media_access_interval_ =
    base::TimeDelta::FromSecondsD(0.2);

constexpr base::TimeDelta kExitVrDueToUnsupportedModeDelay =
    base::TimeDelta::FromSeconds(5);

constexpr base::TimeDelta kAssetsComponentWaitDelay =
    base::TimeDelta::FromSeconds(2);

static constexpr float kInchesToMeters = 0.0254f;
// Screen pixel density of the Google Pixel phone in pixels per inch.
static constexpr float kPixelPpi = 441.0f;
// Screen pixel density of the Google Pixel phone in pixels per meter.
static constexpr float kPixelPpm = kPixelPpi / kInchesToMeters;

// Factor to adjust the legibility of the content. Making this factor smaller
// increases the text size.
static constexpr float kContentLegibilityFactor = 1.36f;

// This factor converts the physical width of the projected
// content quad into the required width of the virtual content window.
// TODO(tiborg): This value is calibrated for the Google Pixel. We should adjust
// this value dynamically based on the target device's pixel density in the
// future.
static constexpr float kContentBoundsMetersToWindowSize =
    kPixelPpm * kContentLegibilityFactor;

// Factor by which the content's pixel amount is increased beyond what the
// projected content quad covers in screen real estate.
// This DPR factor works well on Pixel phones.
static constexpr float kContentDprFactor = 4.0f;

void SetIsInVR(content::WebContents* contents, bool is_in_vr) {
  if (contents && contents->GetRenderWidgetHostView()) {
    // TODO(asimjour) Contents should not be aware of VR mode. Instead, we
    // should add a flag for disabling specific UI such as the keyboard (see
    // VrTabHelper for details).
    contents->GetRenderWidgetHostView()->SetIsInVR(is_in_vr);

    VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
    DCHECK(vr_tab_helper);
    vr_tab_helper->SetIsInVr(is_in_vr);
  }
}

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 const UiInitialState& ui_initial_state,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering,
                 float display_width_meters,
                 float display_height_meters,
                 int display_width_pixels,
                 int display_height_pixels,
                 bool pause_content)
    : vr_shell_enabled_(base::FeatureList::IsEnabled(features::kVrBrowsing)),
      web_vr_autopresentation_expected_(
          ui_initial_state.web_vr_autopresentation_expected),
      delegate_provider_(delegate),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reprojected_rendering_(reprojected_rendering),
      display_size_meters_(display_width_meters, display_height_meters),
      display_size_pixels_(display_width_pixels, display_height_pixels),
      waiting_for_assets_component_timer_(false, false),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  DCHECK(g_vr_shell_instance == nullptr);
  g_vr_shell_instance = this;
  j_vr_shell_.Reset(env, obj);

  gl_thread_ = std::make_unique<VrGLThread>(
      weak_ptr_factory_.GetWeakPtr(), main_thread_task_runner_, gvr_api,
      ui_initial_state, reprojected_rendering_, HasDaydreamSupport(env),
      pause_content);
  ui_ = gl_thread_.get();
  toolbar_ = std::make_unique<ToolbarHelper>(ui_, this);
  autocomplete_controller_ =
      std::make_unique<AutocompleteController>(base::BindRepeating(
          &BrowserUiInterface::SetOmniboxSuggestions, base::Unretained(ui_)));

  gl_thread_->Start();

  if (ui_initial_state.in_web_vr ||
      ui_initial_state.web_vr_autopresentation_expected) {
    UMA_HISTOGRAM_BOOLEAN("VRAutopresentedWebVR",
                          ui_initial_state.web_vr_autopresentation_expected);
  }

  if (AssetsLoader::GetInstance()->ComponentReady()) {
    LoadAssets();
  } else {
    waiting_for_assets_component_timer_.Start(
        FROM_HERE, kAssetsComponentWaitDelay,
        base::BindRepeating(&VrShell::OnAssetsComponentWaitTimeout,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  AssetsLoader::GetInstance()->SetOnComponentReadyCallback(base::BindRepeating(
      &VrShell::OnAssetsComponentReady, weak_ptr_factory_.GetWeakPtr()));
  AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(Mode::kVr);

  UpdateVrAssetsComponent(g_browser_process->component_updater());

  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(device::mojom::kServiceName, &geolocation_config_);
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShell::SwapContents(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           const JavaParamRef<jobject>& tab) {
  content_id_++;
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::OnSwapContents,
                                gl_thread_->GetVrShellGl(), content_id_));
  TabAndroid* active_tab =
      tab.is_null()
          ? nullptr
          : TabAndroid::GetNativeTab(env, JavaParamRef<jobject>(env, tab));

  content::WebContents* contents =
      active_tab ? active_tab->web_contents() : nullptr;
  bool is_native_page = active_tab ? active_tab->IsNativePage() : true;

  SetIsInVR(GetNonNativePageWebContents(), false);

  web_contents_ = contents;
  web_contents_is_native_page_ = is_native_page;
  SetIsInVR(GetNonNativePageWebContents(), true);
  SetUiState();

  if (web_contents_) {
    vr_input_connection_.reset(new VrInputConnection(web_contents_));
    PostToGlThread(FROM_HERE,
                   base::BindOnce(&VrGLThread::SetInputConnection,
                                  base::Unretained(gl_thread_.get()),
                                  vr_input_connection_->GetWeakPtr()));
  } else {
    vr_input_connection_ = nullptr;
  }

  vr_web_contents_observer_ = std::make_unique<VrWebContentsObserver>(
      web_contents_, this, ui_, toolbar_.get());

  if (!GetNonNativePageWebContents()) {
    metrics_helper_ = nullptr;
    return;
  }

  // TODO(billorr): Make VrMetricsHelper tab-aware and able to track multiple
  // tabs. https://crbug.com/684661
  metrics_helper_ = std::make_unique<VrMetricsHelper>(
      GetNonNativePageWebContents(),
      webvr_mode_ ? Mode::kWebXrVrPresentation : Mode::kVrBrowsingRegular,
      web_vr_autopresentation_expected_);
}

void VrShell::SetAndroidGestureTarget(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& android_ui_gesture_target) {
  android_ui_gesture_target_.reset(
      AndroidUiGestureTarget::FromJavaObject(android_ui_gesture_target));
}

void VrShell::SetDialogGestureTarget(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& dialog_gesture_target) {
  dialog_gesture_target_.reset(
      AndroidUiGestureTarget::FromJavaObject(dialog_gesture_target));
}

void VrShell::SetUiState() {
  toolbar_->Update();

  if (!GetNonNativePageWebContents()) {
    ui_->SetLoading(false);
    ui_->SetFullscreen(false);
  } else {
    ui_->SetLoading(GetNonNativePageWebContents()->IsLoading());
    ui_->SetFullscreen(GetNonNativePageWebContents()->IsFullscreen());
  }
  if (web_contents_) {
    ui_->SetIncognito(web_contents_->GetBrowserContext()->IsOffTheRecord());
  } else {
    ui_->SetIncognito(false);
  }
}

VrShell::~VrShell() {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  content_surface_texture_ = nullptr;
  overlay_surface_texture_ = nullptr;
  poll_capturing_media_task_.Cancel();
  if (gvr_gamepad_source_active_) {
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_GVR);
  }

  if (cardboard_gamepad_source_active_) {
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_CARDBOARD);
  }

  delegate_provider_->RemoveDelegate();
  {
    // The GvrLayout is, and must always be, used only on the UI thread, and the
    // GvrApi used for rendering should only be used from the GL thread as it's
    // not thread safe. However, the GvrLayout owns the GvrApi instance, and
    // when it gets shut down it deletes the GvrApi instance with it. Therefore,
    // we need to block shutting down the GvrLayout on stopping our GL thread
    // from using the GvrApi instance.
    // base::Thread::Stop, which is called when destroying the thread, asserts
    // that IO is allowed to prevent jank, but there shouldn't be any concerns
    // regarding jank in this case, because we're switching from 3D to 2D,
    // adding/removing a bunch of Java views, and probably changing device
    // orientation here.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    gl_thread_.reset();
  }
  g_vr_shell_instance = nullptr;
}

void VrShell::PostToGlThread(const base::Location& from_here,
                             base::OnceClosure task) {
  gl_thread_->message_loop()->task_runner()->PostTask(from_here,
                                                      std::move(task));
}

void VrShell::OnContentPaused(bool paused) {
  if (!vr_shell_enabled_)
    return;
  device::VRDevice* device = delegate_provider_->GetDevice();
  if (!device)
    return;

  // TODO(mthiesse): The page is no longer visible when in menu mode. We
  // should unfocus or otherwise let it know it's hidden.
  if (paused)
    device->Blur();
  else
    device->Focus();
}

void VrShell::Navigate(GURL url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_loadUrl(
      env, j_vr_shell_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

void VrShell::NavigateBack() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_navigateBack(env, j_vr_shell_);
}

void VrShell::ExitCct() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_exitCct(env, j_vr_shell_);
}

void VrShell::CloseHostedDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_closeCurrentDialog(env, j_vr_shell_);
}

void VrShell::ToggleCardboardGamepad(bool enabled) {
  // Enable/disable updating gamepad state.
  if (cardboard_gamepad_source_active_ && !enabled) {
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_CARDBOARD);
    cardboard_gamepad_data_fetcher_ = nullptr;
    cardboard_gamepad_source_active_ = false;
  }

  if (!cardboard_gamepad_source_active_ && enabled) {
    device::VRDevice* device = delegate_provider_->GetDevice();
    if (!device)
      return;

    device::GamepadDataFetcherManager::GetInstance()->AddFactory(
        new device::CardboardGamepadDataFetcher::Factory(this,
                                                         device->GetId()));
    cardboard_gamepad_source_active_ = true;
    if (pending_cardboard_trigger_) {
      OnTriggerEvent(nullptr, JavaParamRef<jobject>(nullptr), true);
    }
    pending_cardboard_trigger_ = false;
  }
}

void VrShell::ToggleGvrGamepad(bool enabled) {
  // Enable/disable updating gamepad state.
  if (enabled) {
    DCHECK(!gvr_gamepad_source_active_);
    device::VRDevice* device = delegate_provider_->GetDevice();
    if (!device)
      return;

    device::GamepadDataFetcherManager::GetInstance()->AddFactory(
        new device::GvrGamepadDataFetcher::Factory(this, device->GetId()));
    gvr_gamepad_source_active_ = true;
  } else {
    DCHECK(gvr_gamepad_source_active_);
    device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
        device::GAMEPAD_SOURCE_GVR);
    gvr_gamepad_data_fetcher_ = nullptr;
    gvr_gamepad_source_active_ = false;
  }
}

void VrShell::OnTriggerEvent(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             bool touched) {
  // If we are running cardboard, update gamepad state.
  if (cardboard_gamepad_source_active_) {
    device::CardboardGamepadData pad;
    pad.timestamp = cardboard_gamepad_timer_++;
    pad.is_screen_touching = touched;
    if (cardboard_gamepad_data_fetcher_) {
      cardboard_gamepad_data_fetcher_->SetGamepadData(pad);
    }
  } else {
    pending_cardboard_trigger_ = touched;
  }
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::OnPause,
                                           gl_thread_->GetVrShellGl()));

  // exit vr session
  if (metrics_helper_)
    metrics_helper_->SetVRActive(false);
  SetIsInVR(GetNonNativePageWebContents(), false);

  poll_capturing_media_task_.Cancel();
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::OnResume,
                                           gl_thread_->GetVrShellGl()));

  if (metrics_helper_)
    metrics_helper_->SetVRActive(true);
  SetIsInVR(GetNonNativePageWebContents(), true);

  PollMediaAccessFlag();
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  CHECK(!reprojected_rendering_);
  if (surface.is_null())
    return;
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::InitializeGl,
                                           gl_thread_->GetVrShellGl(),
                                           base::Unretained(window)));
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           bool enabled,
                           bool show_toast) {
  webvr_mode_ = enabled;
  if (metrics_helper_)
    metrics_helper_->SetWebVREnabled(enabled);
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::SetWebVrMode,
                                gl_thread_->GetVrShellGl(), enabled));
  ui_->SetWebVrMode(enabled, show_toast);

  if (!webvr_mode_ && !web_vr_autopresentation_expected_) {
    AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(Mode::kVrBrowsing);
  } else {
    AssetsLoader::GetInstance()->GetMetricsHelper()->OnEnter(
        Mode::kWebXrVrPresentation);
  }
}

void VrShell::OnFullscreenChanged(bool enabled) {
  ui_->SetFullscreen(enabled);
}

bool VrShell::GetWebVrMode(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return webvr_mode_;
}

bool VrShell::IsDisplayingUrlForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ShouldDisplayURL();
}

void VrShell::OnLoadProgressChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    double progress) {
  ui_->SetLoadProgress(progress);
}

void VrShell::OnTabListCreated(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jobjectArray tabs,
                               jobjectArray incognito_tabs) {
  ProcessTabArray(env, tabs, false);
  ProcessTabArray(env, incognito_tabs, true);
  ui_->FlushTabList();
}

void VrShell::ProcessTabArray(JNIEnv* env, jobjectArray tabs, bool incognito) {
  size_t len = env->GetArrayLength(tabs);
  for (size_t i = 0; i < len; ++i) {
    ScopedJavaLocalRef<jobject> j_tab(env, env->GetObjectArrayElement(tabs, i));
    TabAndroid* tab = TabAndroid::GetNativeTab(env, j_tab);
    ui_->AppendToTabList(incognito, tab->GetAndroidId(), tab->GetTitle());
  }
}

void VrShell::OnTabUpdated(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id,
                           jstring jtitle) {
  std::string title;
  base::android::ConvertJavaStringToUTF8(env, jtitle, &title);
  ui_->UpdateTab(incognito, id, title);
}

void VrShell::OnTabRemoved(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id) {
  ui_->RemoveTab(incognito, id);
}

void VrShell::SetAlertDialog(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int width,
                             int height) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::EnableAlertDialog,
                                           gl_thread_->GetVrShellGl(),
                                           gl_thread_.get(), width, height));
}

void VrShell::CloseAlertDialog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::DisableAlertDialog,
                                           gl_thread_->GetVrShellGl()));
}

void VrShell::SetAlertDialogSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int width,
    int height) {
  if (ui_surface_texture_)
    ui_surface_texture_->SetDefaultBufferSize(width, height);
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::SetAlertDialogSize,
                                gl_thread_->GetVrShellGl(), width, height));
}

void VrShell::ConnectPresentingService(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    device::mojom::VRPresentationProviderRequest request,
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::VRRequestPresentOptionsPtr present_options) {
  PostToGlThread(
      FROM_HERE,
      base::BindOnce(&VrShellGl::ConnectPresentingService,
                     gl_thread_->GetVrShellGl(), submit_client.PassInterface(),
                     std::move(request), std::move(display_info),
                     std::move(present_options)));
}

void VrShell::SetHistoryButtonsEnabled(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean can_go_back,
                                       jboolean can_go_forward) {
  ui_->SetHistoryButtonsEnabled(can_go_back, can_go_forward);
}

void VrShell::RequestToExitVr(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              int reason) {
  ui_->ShowExitVrPrompt(static_cast<UiUnsupportedMode>(reason));
}

void VrShell::ContentSurfaceCreated(jobject surface,
                                    gl::SurfaceTexture* texture) {
  content_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShellImpl_contentSurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::ContentOverlaySurfaceCreated(jobject surface,
                                           gl::SurfaceTexture* texture) {
  overlay_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShellImpl_contentOverlaySurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::DialogSurfaceCreated(jobject surface,
                                   gl::SurfaceTexture* texture) {
  ui_surface_texture_ = texture;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> ref(env, surface);
  Java_VrShellImpl_dialogSurfaceCreated(env, j_vr_shell_, ref);
}

void VrShell::GvrDelegateReady(
    gvr::ViewerType viewer_type,
    device::mojom::VRDisplayFrameTransportOptionsPtr transport_options) {
  frame_transport_options_ = std::move(transport_options);
  delegate_provider_->SetDelegate(this, viewer_type);
}

device::mojom::VRDisplayFrameTransportOptionsPtr
VrShell::GetVRDisplayFrameTransportOptions() {
  // Caller takes ownership. Must return a copy to support having this called
  // multiple times during the lifetime of this instance.
  return frame_transport_options_.Clone();
}

void VrShell::BufferBoundsChanged(JNIEnv* env,
                                  const JavaParamRef<jobject>& object,
                                  jint content_width,
                                  jint content_height,
                                  jint overlay_width,
                                  jint overlay_height) {
  TRACE_EVENT0("gpu", "VrShell::ContentPhysicalBoundsChanged");
  if (content_surface_texture_) {
    content_surface_texture_->SetDefaultBufferSize(content_width,
                                                   content_height);
  }
  if (overlay_surface_texture_) {
    overlay_surface_texture_->SetDefaultBufferSize(overlay_width,
                                                   overlay_height);
  }
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::BufferBoundsChanged,
                                gl_thread_->GetVrShellGl(),
                                gfx::Size(content_width, content_height),
                                gfx::Size(overlay_width, overlay_height)));
}

void VrShell::ResumeContentRendering(JNIEnv* env,
                                     const JavaParamRef<jobject>& object) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::ResumeContentRendering,
                                           gl_thread_->GetVrShellGl()));
}

void VrShell::ContentWebContentsDestroyed() {
  web_contents_ = nullptr;
}

void VrShell::ForceExitVr() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_forceExitVr(env, j_vr_shell_);
}

void VrShell::ExitPresent() {
  delegate_provider_->ExitWebVRPresent();
}

void VrShell::ExitFullscreen() {
  if (GetNonNativePageWebContents() &&
      GetNonNativePageWebContents()->IsFullscreen()) {
    GetNonNativePageWebContents()->ExitFullscreen(false);
  }
}

void VrShell::LogUnsupportedModeUserMetric(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           int mode) {
  LogUnsupportedModeUserMetric((UiUnsupportedMode)mode);
}

void VrShell::ShowSoftInput(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            bool show) {
  ui_->ShowSoftInput(show);
}

void VrShell::UpdateWebInputIndices(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int selection_start,
    int selection_end,
    int composition_start,
    int composition_end) {
  PostToGlThread(FROM_HERE, base::BindOnce(&VrGLThread::UpdateWebInputIndices,
                                           base::Unretained(gl_thread_.get()),
                                           selection_start, selection_end,
                                           composition_start, composition_end));
}

void VrShell::LogUnsupportedModeUserMetric(UiUnsupportedMode mode) {
  UMA_HISTOGRAM_ENUMERATION("VR.Shell.EncounteredUnsupportedMode", mode,
                            UiUnsupportedMode::kCount);
}

void VrShell::ExitVrDueToUnsupportedMode(UiUnsupportedMode mode) {
  ui_->SetIsExiting();
  PostToGlThread(FROM_HERE, base::BindOnce(&VrShellGl::set_is_exiting,
                                           gl_thread_->GetVrShellGl(), true));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&VrShell::ForceExitVr, weak_ptr_factory_.GetWeakPtr()),
      kExitVrDueToUnsupportedModeDelay);
  LogUnsupportedModeUserMetric(mode);
}

content::WebContents* VrShell::GetNonNativePageWebContents() const {
  return !web_contents_is_native_page_ ? web_contents_ : nullptr;
}

void VrShell::OnUnsupportedMode(UiUnsupportedMode mode) {
  switch (mode) {
    case UiUnsupportedMode::kUnhandledCodePoint:  // Fall through.
    case UiUnsupportedMode::kGenericUnsupportedFeature:
      ExitVrDueToUnsupportedMode(mode);
      return;
    case UiUnsupportedMode::kUnhandledPageInfo: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShellImpl_onUnhandledPageInfo(env, j_vr_shell_);
      return;
    }
    case UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShellImpl_onUnhandledPermissionPrompt(env, j_vr_shell_);
      return;
    }
    case UiUnsupportedMode::kNeedsKeyboardUpdate: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShellImpl_onNeedsKeyboardUpdate(env, j_vr_shell_);
      return;
    }
    case UiUnsupportedMode::kCount:
      NOTREACHED();  // Should never be used as a mode.
      return;
  }

  NOTREACHED();
  ExitVrDueToUnsupportedMode(mode);
}

void VrShell::OnExitVrPromptResult(UiUnsupportedMode reason,
                                   ExitVrPromptChoice choice) {
  bool should_exit;
  switch (choice) {
    case ExitVrPromptChoice::CHOICE_NONE:
    case ExitVrPromptChoice::CHOICE_STAY:
      should_exit = false;
      break;
    case ExitVrPromptChoice::CHOICE_EXIT:
      should_exit = true;
      break;
  }

  if (reason == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission) {
    // Note that we already measure the number of times the user exits VR
    // because of the record audio permission through
    // VR.Shell.EncounteredUnsupportedMode histogram. This histogram measures
    // whether the user chose to proceed to grant the OS record audio permission
    // through the reported Boolean.
    UMA_HISTOGRAM_BOOLEAN("VR.VoiceSearch.RecordAudioOsPermissionPromptChoice",
                          choice == ExitVrPromptChoice::CHOICE_EXIT);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_onExitVrRequestResult(env, j_vr_shell_,
                                         static_cast<int>(reason), should_exit);
}

void VrShell::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {
  // We have to fit the content into the portion of the display for one eye.
  // Thus, take only half the width.
  int width_pixels = display_size_pixels_.width() / 2;
  float width_meters = display_size_meters_.width() / 2;

  // The physical and pixel dimensions to draw the scene for one eye needs to be
  // a square so that the content's aspect ratio is preserved (i.e. make pixels
  // square for the calculations).

  // For the resolution err on the side of too many pixels so that our content
  // is rather drawn with too high of a resolution than too low.
  int length_pixels = std::max(width_pixels, display_size_pixels_.height());

  // For the size err on the side of a too small area so that the font size is
  // rather too big than too small.
  float length_meters = std::min(width_meters, display_size_meters_.height());

  // Calculate the virtual window size and DPR and pass this to VrShellImpl.
  gfx::Size window_size = gfx::ToRoundedSize(
      gfx::ScaleSize(bounds, width_meters * kContentBoundsMetersToWindowSize));
  // Need to use sqrt(kContentDprFactor) to translate from a factor applicable
  // to the area to a factor applicable to one side length.
  float dpr =
      (length_pixels / (length_meters * kContentBoundsMetersToWindowSize)) *
      std::sqrt(kContentDprFactor);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_setContentCssSize(env, j_vr_shell_, window_size.width(),
                                     window_size.height(), dpr);

  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::ContentBoundsChanged,
                                gl_thread_->GetVrShellGl(), window_size.width(),
                                window_size.height()));
}

void VrShell::SetVoiceSearchActive(bool active) {
  if (!active && !speech_recognizer_)
    return;

  if (!HasAudioPermission()) {
    OnUnsupportedMode(
        UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
    return;
  }

  if (!speech_recognizer_) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    std::string profile_locale = g_browser_process->GetApplicationLocale();
    speech_recognizer_.reset(new SpeechRecognizer(
        this, ui_, profile->GetRequestContext(), profile_locale));
  }
  if (active) {
    speech_recognizer_->Start();
    if (metrics_helper_)
      metrics_helper_->RecordVoiceSearchStarted();
  } else {
    speech_recognizer_->Stop();
  }
}

void VrShell::StartAutocomplete(const AutocompleteRequest& request) {
  autocomplete_controller_->Start(request);
}

void VrShell::StopAutocomplete() {
  autocomplete_controller_->Stop();
}

bool VrShell::HasAudioPermission() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_VrShellImpl_hasAudioPermission(env, j_vr_shell_);
}

void VrShell::PollMediaAccessFlag() {
  poll_capturing_media_task_.Cancel();

  poll_capturing_media_task_.Reset(base::BindRepeating(
      &VrShell::PollMediaAccessFlag, base::Unretained(this)));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE, poll_capturing_media_task_.callback(),
      poll_media_access_interval_);

  int num_tabs_capturing_audio = 0;
  int num_tabs_capturing_video = 0;
  int num_tabs_capturing_screen = 0;
  int num_tabs_bluetooth_connected = 0;
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator();

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (!rvh)
      continue;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents)
      continue;
    if (web_contents->GetRenderViewHost() != rvh)
      continue;
    // Because a WebContents can only have one current RVH at a time, there will
    // be no duplicate WebContents here.
    if (indicator->IsCapturingAudio(web_contents))
      num_tabs_capturing_audio++;
    if (indicator->IsCapturingVideo(web_contents))
      num_tabs_capturing_video++;
    if (indicator->IsBeingMirrored(web_contents))
      num_tabs_capturing_screen++;
    if (web_contents->IsConnectedToBluetoothDevice())
      num_tabs_bluetooth_connected++;
  }

  geolocation_config_->IsHighAccuracyLocationBeingCaptured(base::BindRepeating(
      &VrShell::SetHighAccuracyLocation, base::Unretained(this)));

  bool is_capturing_audio = num_tabs_capturing_audio > 0;
  bool is_capturing_video = num_tabs_capturing_video > 0;
  bool is_capturing_screen = num_tabs_capturing_screen > 0;
  bool is_bluetooth_connected = num_tabs_bluetooth_connected > 0;
  if (is_capturing_audio != is_capturing_audio_) {
    ui_->SetAudioCaptureEnabled(is_capturing_audio);
    is_capturing_audio_ = is_capturing_audio;
  }
  if (is_capturing_video != is_capturing_video_) {
    ui_->SetVideoCaptureEnabled(is_capturing_video);
    is_capturing_video_ = is_capturing_video;
  }
  if (is_capturing_screen != is_capturing_screen_) {
    ui_->SetScreenCaptureEnabled(is_capturing_screen);
    is_capturing_screen_ = is_capturing_screen;
  }
  if (is_bluetooth_connected != is_bluetooth_connected_) {
    ui_->SetBluetoothConnected(is_bluetooth_connected);
    is_bluetooth_connected_ = is_bluetooth_connected;
  }
}

void VrShell::SetHighAccuracyLocation(bool high_accuracy_location) {
  if (high_accuracy_location == high_accuracy_location_)
    return;
  ui_->SetLocationAccessEnabled(high_accuracy_location);
  high_accuracy_location_ = high_accuracy_location;
}

void VrShell::ClearFocusedElement() {
  if (!web_contents_)
    return;

  web_contents_->ClearFocusedElement();
}

void VrShell::ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event,
                                    int content_id) {
  // Block the events if they don't belong to the current content
  if (content_id_ != content_id)
    return;

  if (!android_ui_gesture_target_)
    return;

  android_ui_gesture_target_->DispatchWebInputEvent(std::move(event));
}

void VrShell::ProcessDialogGesture(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!dialog_gesture_target_)
    return;

  dialog_gesture_target_->DispatchWebInputEvent(std::move(event));
}

void VrShell::UpdateGamepadData(device::GvrGamepadData pad) {
  if (gvr_gamepad_source_active_ != pad.connected)
    ToggleGvrGamepad(pad.connected);

  if (gvr_gamepad_data_fetcher_)
    gvr_gamepad_data_fetcher_->SetGamepadData(pad);
}

void VrShell::RegisterGvrGamepadDataFetcher(
    device::GvrGamepadDataFetcher* fetcher) {
  DVLOG(1) << __FUNCTION__ << "(" << fetcher << ")";
  gvr_gamepad_data_fetcher_ = fetcher;
}

void VrShell::RegisterCardboardGamepadDataFetcher(
    device::CardboardGamepadDataFetcher* fetcher) {
  DVLOG(1) << __FUNCTION__ << "(" << fetcher << ")";
  cardboard_gamepad_data_fetcher_ = fetcher;
}

bool VrShell::HasDaydreamSupport(JNIEnv* env) {
  return Java_VrShellImpl_hasDaydreamSupport(env, j_vr_shell_);
}

content::WebContents* VrShell::GetActiveWebContents() const {
  // TODO(tiborg): Handle the case when Tab#isShowingErrorPage returns true.
  return web_contents_;
}

bool VrShell::ShouldDisplayURL() const {
  content::NavigationEntry* entry = GetNavigationEntry();
  if (!entry) {
    return ChromeToolbarModelDelegate::ShouldDisplayURL();
  }
  GURL url = entry->GetVirtualURL();
  // URL is of the form chrome-native://.... This is not useful for the user.
  // Hide it.
  if (url.SchemeIs(chrome::kChromeUINativeScheme)) {
    return false;
  }
  // URL is of the form chrome://....
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return true;
  }
  return ChromeToolbarModelDelegate::ShouldDisplayURL();
}

void VrShell::OnVoiceResults(const base::string16& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  GURL url;
  bool input_was_url;
  std::tie(url, input_was_url) =
      autocomplete_controller_->GetUrlFromVoiceInput(result);

  // TODO(http://crbug.com/817559): If the user is doing a voice search from the
  // new tab page, no metrics data is recorded (including voice search started).
  // Fix this.

  // This should happen before the load to avoid concurency issues.
  if (metrics_helper_ && input_was_url)
    metrics_helper_->RecordUrlRequestedByVoice(url);

  Java_VrShellImpl_loadUrl(
      env, j_vr_shell_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

void VrShell::OnAssetsLoaded(AssetsLoadStatus status,
                             std::unique_ptr<Assets> assets,
                             const base::Version& component_version) {
  ui_->OnAssetsLoaded(status, std::move(assets), component_version);

  if (status == AssetsLoadStatus::kSuccess) {
    VLOG(1) << "Successfully loaded VR assets component";
  } else {
    VLOG(1) << "Failed to load VR assets component";
  }

  AssetsLoader::GetInstance()->GetMetricsHelper()->OnAssetsLoaded(
      status, component_version);
}

void VrShell::LoadAssets() {
  AssetsLoader::GetInstance()->Load(
      base::BindOnce(&VrShell::OnAssetsLoaded, base::Unretained(this)));
}

void VrShell::OnAssetsComponentReady() {
  // We don't apply updates after the timer expires because that would lead to
  // replacing the user's environment. New updates will be applied when
  // re-entering VR.
  if (waiting_for_assets_component_timer_.IsRunning()) {
    waiting_for_assets_component_timer_.Stop();
    LoadAssets();
  }
}

void VrShell::OnAssetsComponentWaitTimeout() {
  ui_->OnAssetsUnavailable();
}

void VrShell::AcceptDoffPromptForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE,
                 base::BindOnce(&VrShellGl::AcceptDoffPromptForTesting,
                                gl_thread_->GetVrShellGl()));
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong JNI_VrShellImpl_Init(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           const JavaParamRef<jobject>& delegate,
                           jboolean for_web_vr,
                           jboolean web_vr_autopresentation_expected,
                           jboolean in_cct,
                           jboolean browsing_disabled,
                           jboolean has_or_can_request_audio_permission,
                           jlong gvr_api,
                           jboolean reprojected_rendering,
                           jfloat display_width_meters,
                           jfloat display_height_meters,
                           jint display_width_pixels,
                           jint display_pixel_height,
                           jboolean pause_content) {
  UiInitialState ui_initial_state;
  ui_initial_state.browsing_disabled = browsing_disabled;
  ui_initial_state.in_cct = in_cct;
  ui_initial_state.in_web_vr = for_web_vr;
  ui_initial_state.web_vr_autopresentation_expected =
      web_vr_autopresentation_expected;
  ui_initial_state.has_or_can_request_audio_permission =
      has_or_can_request_audio_permission;
  ui_initial_state.skips_redraw_when_not_dirty =
      base::FeatureList::IsEnabled(features::kVrBrowsingExperimentalRendering);
  ui_initial_state.assets_supported = AssetsLoader::AssetsSupported();

  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, ui_initial_state,
      VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering,
      display_width_meters, display_height_meters, display_width_pixels,
      display_pixel_height, pause_content));
}

}  // namespace vr
