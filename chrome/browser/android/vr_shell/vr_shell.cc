// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include <android/native_window_jni.h>

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr_shell/android_ui_gesture_target.h"
#include "chrome/browser/android/vr_shell/vr_compositor.h"
#include "chrome/browser/android/vr_shell/vr_gl_thread.h"
#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "chrome/browser/android/vr_shell/vr_web_contents_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/vr/toolbar_helper.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/vr/web_contents_event_forwarder.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "device/geolocation/public/interfaces/geolocation_config.mojom.h"
#include "device/vr/android/gvr/cardboard_gamepad_data_fetcher.h"
#include "device/vr/android/gvr/gvr_gamepad_data_fetcher.h"
#include "device/vr/vr_device.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "jni/VrShellImpl_jni.h"
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
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace vr_shell {

namespace {
vr_shell::VrShell* g_instance;

constexpr base::TimeDelta poll_media_access_interval_ =
    base::TimeDelta::FromSecondsD(0.2);

constexpr base::TimeDelta kExitVrDueToUnsupportedModeDelay =
    base::TimeDelta::FromSeconds(5);

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

    vr::VrTabHelper* vr_tab_helper = vr::VrTabHelper::FromWebContents(contents);
    DCHECK(vr_tab_helper);
    vr_tab_helper->SetIsInVr(is_in_vr);
  }
}

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 ui::WindowAndroid* window,
                 const vr::UiInitialState& ui_initial_state,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering,
                 float display_width_meters,
                 float display_height_meters,
                 int display_width_pixels,
                 int display_height_pixels)
    : vr_shell_enabled_(base::FeatureList::IsEnabled(features::kVrShell)),
      window_(window),
      compositor_(base::MakeUnique<VrCompositor>(window_)),
      delegate_provider_(delegate),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reprojected_rendering_(reprojected_rendering),
      display_size_meters_(display_width_meters, display_height_meters),
      display_size_pixels_(display_width_pixels, display_height_pixels),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << "=" << this;
  DCHECK(g_instance == nullptr);
  g_instance = this;
  j_vr_shell_.Reset(env, obj);

  gl_thread_ = base::MakeUnique<VrGLThread>(
      weak_ptr_factory_.GetWeakPtr(), main_thread_task_runner_, gvr_api,
      ui_initial_state, reprojected_rendering_, HasDaydreamSupport(env));
  ui_ = gl_thread_.get();
  toolbar_ = base::MakeUnique<vr::ToolbarHelper>(ui_, this);

  gl_thread_->Start();

  if (ui_initial_state.in_web_vr ||
      ui_initial_state.web_vr_autopresentation_expected) {
    UMA_HISTOGRAM_BOOLEAN("VRAutopresentedWebVR", !ui_initial_state.in_web_vr);
  }
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShell::SwapContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& tab,
    const JavaParamRef<jobject>& android_ui_gesture_target) {
  DCHECK(tab.obj());
  content_id_++;
  PostToGlThread(FROM_HERE,
                 base::Bind(&VrShellGl::OnSwapContents,
                            gl_thread_->GetVrShellGl(), content_id_));
  TabAndroid* active_tab =
      TabAndroid::GetNativeTab(env, JavaParamRef<jobject>(env, tab));
  DCHECK(active_tab);
  content::WebContents* contents = active_tab->web_contents();
  bool is_native_page = active_tab->IsNativePage();

  AndroidUiGestureTarget* target = nullptr;
  if (is_native_page) {
    DCHECK(!android_ui_gesture_target.is_null());
    target = AndroidUiGestureTarget::FromJavaObject(android_ui_gesture_target);
  }

  if (contents == web_contents_ && target == android_ui_gesture_target_.get()) {
    return;
  }

  SetIsInVR(GetNonNativePageWebContents(), false);
  web_contents_ = contents;
  web_contents_is_native_page_ = is_native_page;
  compositor_->SetLayer(GetNonNativePageWebContents());
  SetIsInVR(GetNonNativePageWebContents(), true);
  ContentFrameWasResized(false /* unused */);
  SetUiState();

  vr_web_contents_observer_ = base::MakeUnique<VrWebContentsObserver>(
      web_contents_, this, ui_, toolbar_.get());

  if (target) {
    android_ui_gesture_target_.reset(target);
    web_contents_event_forwarder_ = nullptr;
    metrics_helper_ = nullptr;
    return;
  }
  web_contents_event_forwarder_ =
      base::MakeUnique<vr::WebContentsEventForwarder>(
          GetNonNativePageWebContents());
  // TODO(billorr): Make VrMetricsHelper tab-aware and able to track multiple
  // tabs. crbug.com/684661
  metrics_helper_ =
      base::MakeUnique<VrMetricsHelper>(GetNonNativePageWebContents());
  metrics_helper_->SetVRActive(true);
  metrics_helper_->SetWebVREnabled(webvr_mode_);
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
  g_instance = nullptr;
}

void VrShell::PostToGlThread(const base::Location& from_here,
                             const base::Closure& task) {
  gl_thread_->message_loop()->task_runner()->PostTask(from_here, task);
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

void VrShell::NavigateBack() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_navigateBack(env, j_vr_shell_);
}

void VrShell::ExitCct() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_exitCct(env, j_vr_shell_);
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
  PostToGlThread(FROM_HERE,
                 base::Bind(&VrShellGl::OnPause, gl_thread_->GetVrShellGl()));

  // exit vr session
  if (metrics_helper_)
    metrics_helper_->SetVRActive(false);
  SetIsInVR(GetNonNativePageWebContents(), false);

  poll_capturing_media_task_.Cancel();
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PostToGlThread(FROM_HERE,
                 base::Bind(&VrShellGl::OnResume, gl_thread_->GetVrShellGl()));

  if (metrics_helper_)
    metrics_helper_->SetVRActive(true);
  SetIsInVR(GetNonNativePageWebContents(), true);

  PollMediaAccessFlag();
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  CHECK(!reprojected_rendering_);
  if (!surface) {
    compositor_->SurfaceDestroyed();
    return;
  }
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  PostToGlThread(FROM_HERE, base::Bind(&VrShellGl::InitializeGl,
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
  PostToGlThread(FROM_HERE, base::Bind(&VrShellGl::SetWebVrMode,
                                       gl_thread_->GetVrShellGl(), enabled));
  ui_->SetWebVrMode(enabled, show_toast);
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
    jobject jtab = env->GetObjectArrayElement(tabs, i);
    TabAndroid* tab =
        TabAndroid::GetNativeTab(env, JavaParamRef<jobject>(env, jtab));
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

void VrShell::ConnectPresentingService(
    device::mojom::VRSubmitFrameClientPtr submit_client,
    device::mojom::VRPresentationProviderRequest request,
    device::mojom::VRDisplayInfoPtr display_info) {
  PostToGlThread(
      FROM_HERE,
      base::Bind(&VrShellGl::ConnectPresentingService,
                 gl_thread_->GetVrShellGl(),
                 base::Passed(submit_client.PassInterface()),
                 base::Passed(&request), base::Passed(&display_info)));
}

base::android::ScopedJavaGlobalRef<jobject> VrShell::TakeContentSurface(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!content_surface_) {
    return base::android::ScopedJavaGlobalRef<jobject>(env, nullptr);
  }
  taken_surface_ = true;
  compositor_->SurfaceChanged(nullptr);
  base::android::ScopedJavaGlobalRef<jobject> surface(env, content_surface_);
  content_surface_ = nullptr;
  return surface;
}

void VrShell::RestoreContentSurface(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  // Don't try to restore the surface if we haven't successfully taken it yet.
  if (!taken_surface_)
    return;
  taken_surface_ = false;
  PostToGlThread(FROM_HERE, base::Bind(&VrShellGl::CreateContentSurface,
                                       gl_thread_->GetVrShellGl()));
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
  ui_->SetExitVrPromptEnabled(true, (vr::UiUnsupportedMode)reason);
}

void VrShell::ContentSurfaceChanged(jobject surface) {
  content_surface_ = surface;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_contentSurfaceChanged(env, j_vr_shell_);
  compositor_->SurfaceChanged(content_surface_);
}

void VrShell::GvrDelegateReady(gvr::ViewerType viewer_type) {
  delegate_provider_->SetDelegate(this, viewer_type);
}

void VrShell::OnPhysicalBackingSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    jint width,
    jint height) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  web_contents->GetNativeView()->OnSizeChanged(width, height);
  gfx::Size size(width, height);
  web_contents->GetNativeView()->OnPhysicalBackingSizeChanged(size);
}

void VrShell::ContentPhysicalBoundsChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& object,
                                           jint width,
                                           jint height,
                                           jfloat dpr) {
  TRACE_EVENT0("gpu", "VrShell::ContentPhysicalBoundsChanged");
  // TODO(acondor): Set the device scale factor for font rendering on the
  // VR Shell textures.
  PostToGlThread(FROM_HERE,
                 base::Bind(&VrShellGl::ContentPhysicalBoundsChanged,
                            gl_thread_->GetVrShellGl(), width, height));
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

// Note that the following code is obsolete and is here as reference for the
// actions that need to be implemented natively.
void VrShell::DoUiAction(const UiAction action,
                         const base::DictionaryValue* arguments) {
  // Actions that can be handled natively.
  switch (action) {
    default:
      break;
  }
  // Actions that are handled in java.
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (action) {
    case SHOW_TAB: {
      int id;
      CHECK(arguments->GetInteger("id", &id));
      Java_VrShellImpl_showTab(env, j_vr_shell_, id);
      return;
    }
    case OPEN_NEW_TAB: {
      bool incognito;
      CHECK(arguments->GetBoolean("incognito", &incognito));
      Java_VrShellImpl_openNewTab(env, j_vr_shell_, incognito);
      return;
    }
    case HISTORY_FORWARD:
      Java_VrShellImpl_navigateForward(env, j_vr_shell_);
      break;
    case RELOAD:
      Java_VrShellImpl_reload(env, j_vr_shell_);
      break;
    default:
      NOTREACHED();
  }
}

void VrShell::ContentFrameWasResized(bool width_changed) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  PostToGlThread(
      FROM_HERE,
      base::Bind(&VrShellGl::ContentBoundsChanged, gl_thread_->GetVrShellGl(),
                 display.size().width(), display.size().height()));
}

void VrShell::ContentWebContentsDestroyed() {
  web_contents_event_forwarder_.reset();
  web_contents_ = nullptr;
  // TODO(mthiesse): Handle web contents being destroyed.
  ForceExitVr();
}

void VrShell::ContentWasHidden() {
  // Ensure we don't continue sending input to it.
  web_contents_event_forwarder_ = nullptr;
}

void VrShell::ContentWasShown() {
  if (GetNonNativePageWebContents()) {
    web_contents_event_forwarder_ =
        base::MakeUnique<vr::WebContentsEventForwarder>(
            GetNonNativePageWebContents());
  }
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
  LogUnsupportedModeUserMetric((vr::UiUnsupportedMode)mode);
}

void VrShell::LogUnsupportedModeUserMetric(vr::UiUnsupportedMode mode) {
  UMA_HISTOGRAM_ENUMERATION("VR.Shell.EncounteredUnsupportedMode", mode,
                            vr::UiUnsupportedMode::kCount);
}

void VrShell::ExitVrDueToUnsupportedMode(vr::UiUnsupportedMode mode) {
  ui_->SetIsExiting();
  PostToGlThread(FROM_HERE, base::Bind(&VrShellGl::set_is_exiting,
                                       gl_thread_->GetVrShellGl(), true));
  main_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&VrShell::ForceExitVr, weak_ptr_factory_.GetWeakPtr()),
      kExitVrDueToUnsupportedModeDelay);
  LogUnsupportedModeUserMetric(mode);
}

content::WebContents* VrShell::GetNonNativePageWebContents() const {
  return !web_contents_is_native_page_ ? web_contents_ : nullptr;
}

void VrShell::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  switch (mode) {
    case vr::UiUnsupportedMode::kUnhandledPageInfo: {
      JNIEnv* env = base::android::AttachCurrentThread();
      Java_VrShellImpl_onUnhandledPageInfo(env, j_vr_shell_);
      break;
    }
    default:
      ExitVrDueToUnsupportedMode(mode);
      break;
  }
}

void VrShell::OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                                   vr::ExitVrPromptChoice choice) {
  bool should_exit;
  switch (choice) {
    case vr::ExitVrPromptChoice::CHOICE_NONE:
    case vr::ExitVrPromptChoice::CHOICE_STAY:
      ui_->SetExitVrPromptEnabled(false, vr::UiUnsupportedMode::kCount);
      should_exit = false;
      break;
    case vr::ExitVrPromptChoice::CHOICE_EXIT:
      should_exit = true;
      break;
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
}

void VrShell::SetVoiceSearchActive(bool active) {
  if (!active && !speech_recognizer_)
    return;

  if (!speech_recognizer_) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    std::string profile_locale = g_browser_process->GetApplicationLocale();
    speech_recognizer_.reset(new vr::SpeechRecognizer(
        this, profile->GetRequestContext(), profile_locale));
  }
  if (active) {
    speech_recognizer_->Start();
  } else {
    speech_recognizer_->Stop();
  }
}

void VrShell::PollMediaAccessFlag() {
  poll_capturing_media_task_.Cancel();

  poll_capturing_media_task_.Reset(
      base::Bind(&VrShell::PollMediaAccessFlag, base::Unretained(this)));
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
  auto* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface("content_browser", &geolocation_config_);

  geolocation_config_->IsHighAccuracyLocationBeingCaptured(
      base::Bind(&VrShell::SetHighAccuracyLocation, base::Unretained(this)));

  bool is_capturing_audio = num_tabs_capturing_audio > 0;
  bool is_capturing_video = num_tabs_capturing_video > 0;
  bool is_capturing_screen = num_tabs_capturing_screen > 0;
  bool is_bluetooth_connected = num_tabs_bluetooth_connected > 0;
  if (is_capturing_audio != is_capturing_audio_) {
    ui_->SetAudioCapturingIndicator(is_capturing_audio);
    is_capturing_audio_ = is_capturing_audio;
  }
  if (is_capturing_video != is_capturing_video_) {
    ui_->SetVideoCapturingIndicator(is_capturing_video);
    is_capturing_video_ = is_capturing_video;
  }
  if (is_capturing_screen != is_capturing_screen_) {
    ui_->SetScreenCapturingIndicator(is_capturing_screen);
    is_capturing_screen_ = is_capturing_screen;
  }
  if (is_bluetooth_connected != is_bluetooth_connected_) {
    ui_->SetBluetoothConnectedIndicator(is_bluetooth_connected);
    is_bluetooth_connected_ = is_bluetooth_connected;
  }
}

void VrShell::SetHighAccuracyLocation(bool high_accuracy_location) {
  if (high_accuracy_location == high_accuracy_location_)
    return;
  ui_->SetLocationAccessIndicator(high_accuracy_location);
  high_accuracy_location_ = high_accuracy_location;
}

void VrShell::SetContentCssSize(float width, float height, float dpr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_setContentCssSize(env, j_vr_shell_, width, height, dpr);
}

void VrShell::ProcessContentGesture(std::unique_ptr<blink::WebInputEvent> event,
                                    int content_id) {
  // Block the events if they don't belong to the current content
  if (content_id_ != content_id)
    return;

  if (web_contents_event_forwarder_) {
    web_contents_event_forwarder_->ForwardEvent(std::move(event));
  } else if (android_ui_gesture_target_) {
    android_ui_gesture_target_->DispatchWebInputEvent(std::move(event));
  }
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
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  GURL url(GetDefaultSearchURLForSearchTerms(template_url_service, result));
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_loadUrl(
      env, j_vr_shell_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& delegate,
           jlong window_android,
           jboolean for_web_vr,
           jboolean web_vr_autopresentation_expected,
           jboolean in_cct,
           jboolean browsing_disabled,
           jlong gvr_api,
           jboolean reprojected_rendering,
           jfloat display_width_meters,
           jfloat display_height_meters,
           jint display_width_pixels,
           jint display_pixel_height) {
  vr::UiInitialState ui_initial_state;
  ui_initial_state.browsing_disabled = browsing_disabled;
  ui_initial_state.in_cct = in_cct;
  ui_initial_state.in_web_vr = for_web_vr;
  ui_initial_state.web_vr_autopresentation_expected =
      web_vr_autopresentation_expected;

  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, reinterpret_cast<ui::WindowAndroid*>(window_android),
      ui_initial_state,
      VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering,
      display_width_meters, display_height_meters, display_width_pixels,
      display_pixel_height));
}

}  // namespace vr_shell
