// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include <android/native_window_jni.h>

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr_shell/android_ui_gesture_target.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/vr_compositor.h"
#include "chrome/browser/android/vr_shell/vr_gl_thread.h"
#include "chrome/browser/android/vr_shell/vr_input_manager.h"
#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"
#include "chrome/browser/android/vr_shell/vr_web_contents_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellImpl_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace vr_shell {

namespace {
vr_shell::VrShell* g_instance;

static const char kVrShellUIURL[] = "chrome://vr-shell-ui";

void SetIsInVR(content::WebContents* contents, bool is_in_vr) {
  if (contents && contents->GetRenderWidgetHostView())
    contents->GetRenderWidgetHostView()->SetIsInVR(is_in_vr);
}

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 jobject obj,
                 ui::WindowAndroid* content_window,
                 content::WebContents* ui_contents,
                 ui::WindowAndroid* ui_window,
                 bool for_web_vr,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering)
    : WebContentsObserver(ui_contents),
      vr_shell_enabled_(base::FeatureList::IsEnabled(features::kVrShell)),
      content_window_(content_window),
      content_compositor_(
          base::MakeUnique<VrCompositor>(content_window_, false)),
      ui_contents_(ui_contents),
      ui_compositor_(base::MakeUnique<VrCompositor>(ui_window, true)),
      delegate_provider_(delegate),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reprojected_rendering_(reprojected_rendering),
      gvr_api_(gvr_api),
      weak_ptr_factory_(this) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
  j_vr_shell_.Reset(env, obj);

  ui_input_manager_ = base::MakeUnique<VrInputManager>(ui_contents_);
  ui_compositor_->SetLayer(ui_contents_);

  gl_thread_ = base::MakeUnique<VrGLThread>(
      weak_ptr_factory_.GetWeakPtr(), delegate_provider_->GetWeakPtr(),
      main_thread_task_runner_, gvr_api, for_web_vr, reprojected_rendering_);

  base::Thread::Options options(base::MessageLoop::TYPE_DEFAULT, 0);
  options.priority = base::ThreadPriority::DISPLAY;
  gl_thread_->StartWithOptions(options);

  html_interface_ = base::MakeUnique<UiInterface>(
      for_web_vr ? UiInterface::Mode::WEB_VR : UiInterface::Mode::STANDARD);
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void VrShell::SwapContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& web_contents,
    const JavaParamRef<jobject>& touch_event_synthesizer) {
  content::WebContents* contents =
      content::WebContents::FromJavaWebContents(web_contents);
  if (contents == main_contents_ &&
      touch_event_synthesizer.obj() == j_motion_event_synthesizer_.obj())
    return;

  SetIsInVR(main_contents_, false);
  j_motion_event_synthesizer_.Reset(env, touch_event_synthesizer);
  main_contents_ = contents;
  content_compositor_->SetLayer(main_contents_);
  SetIsInVR(main_contents_, true);
  ContentFrameWasResized(false /* unused */);
  SetUiState();

  if (!main_contents_) {
    android_ui_gesture_target_ = base::MakeUnique<AndroidUiGestureTarget>(
        j_motion_event_synthesizer_.obj(),
        Java_VrShellImpl_getNativePageScrollRatio(env, j_vr_shell_.obj()));
    content_input_manager_ = nullptr;
    vr_web_contents_observer_ = nullptr;
    metrics_helper_ = nullptr;
    return;
  }
  content_input_manager_ = base::MakeUnique<VrInputManager>(main_contents_);
  vr_web_contents_observer_ = base::MakeUnique<VrWebContentsObserver>(
      main_contents_, html_interface_.get(), this);
  // TODO(billorr): Make VrMetricsHelper tab-aware and able to track multiple
  // tabs. crbug.com/684661
  metrics_helper_ = base::MakeUnique<VrMetricsHelper>(main_contents_);
  metrics_helper_->SetVRActive(true);
  metrics_helper_->SetWebVREnabled(webvr_mode_);
}

void VrShell::SetUiState() {
  if (!main_contents_) {
    // TODO(mthiesse): Properly handle native page URLs.
    html_interface_->SetURL(GURL());
    html_interface_->SetLoading(false);
    html_interface_->SetFullscreen(false);
  } else {
    html_interface_->SetURL(main_contents_->GetVisibleURL());
    html_interface_->SetLoading(main_contents_->IsLoading());
    html_interface_->SetFullscreen(main_contents_->IsFullscreen());
  }
}

void VrShell::LoadUIContent(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  GURL url(kVrShellUIURL);
  ui_contents_->GetController().LoadURL(
      url, content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string(""));
}

bool RegisterVrShell(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VrShell::~VrShell() {
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
  delegate_provider_->RemoveDelegate();
  g_instance = nullptr;
}

void VrShell::PostToGlThreadWhenReady(const base::Closure& task) {
  // TODO(mthiesse): Remove this blocking wait. Queue up events if thread isn't
  // finished starting?
  gl_thread_->WaitUntilThreadStarted();
  gl_thread_->task_runner()->PostTask(FROM_HERE, task);
}

void VrShell::SetContentPaused(bool paused) {
  if (content_paused_ == paused)
    return;
  content_paused_ = paused;

  if (!delegate_provider_->device_provider())
    return;

  // TODO(mthiesse): The page is no longer visible when in menu mode. We
  // should unfocus or otherwise let it know it's hidden.
  if (paused) {
    delegate_provider_->device_provider()->Device()->OnBlur();
  } else {
    delegate_provider_->device_provider()->Device()->OnFocus();
  }
}

void VrShell::OnTriggerEvent(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  gl_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&VrShellGl::OnTriggerEvent, gl_thread_->GetVrShellGl()));
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  gl_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VrShellGl::OnPause, gl_thread_->GetVrShellGl()));

  // exit vr session
  if (metrics_helper_)
    metrics_helper_->SetVRActive(false);
  SetIsInVR(main_contents_, false);
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  gl_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VrShellGl::OnResume, gl_thread_->GetVrShellGl()));

  if (metrics_helper_)
    metrics_helper_->SetVRActive(true);
  SetIsInVR(main_contents_, true);
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  CHECK(!reprojected_rendering_);
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::InitializeGl,
                                     gl_thread_->GetVrShellGl(),
                                     base::Unretained(window)));
}

base::WeakPtr<VrShell> VrShell::GetWeakPtr(
    const content::WebContents* web_contents) {
  // Ensure that the WebContents requesting the VrShell instance is the one
  // we created.
  if (g_instance != nullptr && g_instance->ui_contents_ == web_contents)
    return g_instance->weak_ptr_factory_.GetWeakPtr();
  return base::WeakPtr<VrShell>(nullptr);
}

void VrShell::OnDomContentsLoaded() {
  SetUiState();
  html_interface_->OnDomContentsLoaded();
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool enabled) {
  webvr_mode_ = enabled;
  if (metrics_helper_)
    metrics_helper_->SetWebVREnabled(enabled);
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::SetWebVrMode,
                                     gl_thread_->GetVrShellGl(), enabled));
  html_interface_->SetMode(enabled ? UiInterface::Mode::WEB_VR
                                   : UiInterface::Mode::STANDARD);
}

void VrShell::OnLoadProgressChanged(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    double progress) {
  html_interface_->SetLoadProgress(progress);
}

void VrShell::OnTabListCreated(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jobjectArray tabs,
                               jobjectArray incognito_tabs) {
  html_interface_->InitTabList();
  ProcessTabArray(env, tabs, false);
  ProcessTabArray(env, incognito_tabs, true);
  html_interface_->FlushTabList();
}

void VrShell::ProcessTabArray(JNIEnv* env, jobjectArray tabs, bool incognito) {
  size_t len = env->GetArrayLength(tabs);
  for (size_t i = 0; i < len; ++i) {
    jobject jtab = env->GetObjectArrayElement(tabs, i);
    TabAndroid* tab =
        TabAndroid::GetNativeTab(env, JavaParamRef<jobject>(env, jtab));
    html_interface_->AppendToTabList(incognito, tab->GetAndroidId(),
                                     tab->GetTitle());
  }
}

void VrShell::OnTabUpdated(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id,
                           jstring jtitle) {
  std::string title;
  base::android::ConvertJavaStringToUTF8(env, jtitle, &title);
  html_interface_->UpdateTab(incognito, id, title);
}

void VrShell::OnTabRemoved(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           jboolean incognito,
                           jint id) {
  html_interface_->RemoveTab(incognito, id);
}

void VrShell::SetWebVRSecureOrigin(bool secure_origin) {
  // TODO(cjgrant): Align this state with the logic that drives the omnibox.
  html_interface_->SetWebVRSecureOrigin(secure_origin);
}

void VrShell::SubmitWebVRFrame() {}

void VrShell::UpdateWebVRTextureBounds(int16_t frame_index,
                                       const gvr::Rectf& left_bounds,
                                       const gvr::Rectf& right_bounds) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UpdateWebVRTextureBounds,
                                     gl_thread_->GetVrShellGl(), frame_index,
                                     left_bounds, right_bounds));
}

bool VrShell::SupportsPresentation() {
  return true;
}

void VrShell::ResetPose() {
  gl_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VrShellGl::ResetPose, gl_thread_->GetVrShellGl()));
}

void VrShell::CreateVRDisplayInfo(
    const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
    uint32_t device_id) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::CreateVRDisplayInfo,
                                     gl_thread_->GetVrShellGl(), callback,
                                     device_id));
}

base::android::ScopedJavaGlobalRef<jobject> VrShell::TakeContentSurface(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content_compositor_->SurfaceChanged(nullptr);
  base::android::ScopedJavaGlobalRef<jobject> surface(env, content_surface_);
  content_surface_ = nullptr;
  return surface;
}

void VrShell::RestoreContentSurface(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::CreateContentSurface, gl_thread_->GetVrShellGl()));
}

void VrShell::UiSurfaceChanged(jobject surface) {
  ui_compositor_->SurfaceChanged(surface);
}

void VrShell::ContentSurfaceChanged(jobject surface) {
  content_surface_ = surface;
  content_compositor_->SurfaceChanged(surface);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_contentSurfaceChanged(env, j_vr_shell_.obj());
}

void VrShell::GvrDelegateReady() {
  delegate_provider_->SetDelegate(this, gvr_api_);
}

void VrShell::AppButtonPressed() {
  if (vr_shell_enabled_)
    html_interface_->HandleAppButtonClicked();
}

void VrShell::ContentPhysicalBoundsChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& object,
                                           jint width,
                                           jint height,
                                           jfloat dpr) {
  TRACE_EVENT0("gpu", "VrShell::ContentPhysicalBoundsChanged");
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::ContentPhysicalBoundsChanged,
                                     gl_thread_->GetVrShellGl(), width,
                                     height));
  content_compositor_->SetWindowBounds(gfx::Size(width, height));
}

void VrShell::UIPhysicalBoundsChanged(JNIEnv* env,
                                      const JavaParamRef<jobject>& object,
                                      jint width,
                                      jint height,
                                      jfloat dpr) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UIPhysicalBoundsChanged,
                                     gl_thread_->GetVrShellGl(), width,
                                     height));
  ui_compositor_->SetWindowBounds(gfx::Size(width, height));
}

UiInterface* VrShell::GetUiInterface() {
  return html_interface_.get();
}

void VrShell::UpdateScene(const base::ListValue* args) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UpdateScene,
                                     gl_thread_->GetVrShellGl(),
                                     base::Passed(args->CreateDeepCopy())));
}

void VrShell::DoUiAction(const UiAction action,
                         const base::DictionaryValue* arguments) {
  // Actions that can be handled natively.
  switch (action) {
    case OMNIBOX_CONTENT:
      html_interface_->HandleOmniboxInput(*arguments);
      return;
    case SET_CONTENT_PAUSED: {
      bool paused;
      CHECK(arguments->GetBoolean("paused", &paused));
      SetContentPaused(paused);
      return;
    }
    case HISTORY_BACK:
      if (main_contents_ && main_contents_->IsFullscreen()) {
        main_contents_->ExitFullscreen(false);
        return;
      }
      // Otherwise handle in java.
      break;
    case ZOOM_OUT:  // Not handled yet.
    case ZOOM_IN:   // Not handled yet.
      return;
    case SHOW_TAB: {
      int id;
      CHECK(arguments->GetInteger("id", &id));
      delegate_provider_->ShowTab(id);
      return;
    }
    case OPEN_NEW_TAB: {
      bool incognito;
      CHECK(arguments->GetBoolean("incognito", &incognito));
      delegate_provider_->OpenNewTab(incognito);
      return;
    }
    case KEY_EVENT: {
      int char_value;
      int modifiers = 0;
      arguments->GetInteger("modifiers", &modifiers);
      CHECK(arguments->GetInteger("charValue", &char_value));
      ui_input_manager_->GenerateKeyboardEvent(char_value, modifiers);
      return;
    }
#if defined(ENABLE_VR_SHELL_UI_DEV)
    case RELOAD_UI:
      ui_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
      html_interface_.reset(new UiInterface(UiInterface::Mode::STANDARD));
      SetUiState();
      vr_web_contents_observer_->SetUiInterface(html_interface_.get());
      return;
#endif
    default:
      break;
  }
  // Actions that are handled in java.
  JNIEnv* env = base::android::AttachCurrentThread();
  switch (action) {
    case HISTORY_BACK:
      Java_VrShellImpl_navigateBack(env, j_vr_shell_.obj());
      break;
    case HISTORY_FORWARD:
      Java_VrShellImpl_navigateForward(env, j_vr_shell_.obj());
      break;
    case RELOAD:
      Java_VrShellImpl_reload(env, j_vr_shell_.obj());
      break;
    case LOAD_URL: {
      base::string16 url_string;
      CHECK(arguments->GetString("url", &url_string));
      base::android::ScopedJavaLocalRef<jstring> string =
          base::android::ConvertUTF16ToJavaString(env, url_string);
      Java_VrShellImpl_loadURL(env, j_vr_shell_.obj(), string,
                               ui::PageTransition::PAGE_TRANSITION_TYPED);
      break;
    }
    default:
      NOTREACHED();
  }
}

void VrShell::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  new_host->GetWidget()->GetView()->SetBackgroundColor(SK_ColorTRANSPARENT);
}

void VrShell::MainFrameWasResized(bool width_changed) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          ui_contents_->GetNativeView());
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::UIBoundsChanged, gl_thread_->GetVrShellGl(),
                 display.size().width(), display.size().height()));
}

void VrShell::ContentFrameWasResized(bool width_changed) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(content_window_);
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::ContentBoundsChanged, gl_thread_->GetVrShellGl(),
                 display.size().width(), display.size().height()));
}

void VrShell::WebContentsDestroyed() {
  ui_input_manager_.reset();
  ui_contents_ = nullptr;
  // TODO(mthiesse): Handle web contents being destroyed.
  ForceExitVr();
}

void VrShell::ContentWebContentsDestroyed() {
  content_input_manager_.reset();
  main_contents_ = nullptr;
  // TODO(mthiesse): Handle web contents being destroyed.
  ForceExitVr();
}

void VrShell::ContentWasHidden() {
  // Ensure we don't continue sending input to it.
  content_input_manager_ = nullptr;
}

void VrShell::ContentWasShown() {
  if (main_contents_)
    content_input_manager_ = base::MakeUnique<VrInputManager>(main_contents_);
}

void VrShell::ForceExitVr() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_forceExitVr(env, j_vr_shell_.obj());
}

void VrShell::OnVRVsyncProviderRequest(
    device::mojom::VRVSyncProviderRequest request) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::OnRequest,
                                     gl_thread_->GetVrShellGl(),
                                     base::Passed(&request)));
}

void VrShell::UpdateVSyncInterval(int64_t timebase_nanos,
                                  double interval_seconds) {
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UpdateVSyncInterval,
                                     gl_thread_->GetVrShellGl(), timebase_nanos,
                                     interval_seconds));
}

void VrShell::SetContentCssSize(float width, float height, float dpr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_setContentCssSize(env, j_vr_shell_.obj(), width, height,
                                     dpr);
}

void VrShell::SetUiCssSize(float width, float height, float dpr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VrShellImpl_setUiCssSize(env, j_vr_shell_.obj(), width, height, dpr);
}

void VrShell::ProcessUIGesture(std::unique_ptr<blink::WebInputEvent> event) {
  if (ui_input_manager_) {
    ui_input_manager_->ProcessUpdatedGesture(std::move(event));
  }
}

void VrShell::ProcessContentGesture(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (content_input_manager_) {
    content_input_manager_->ProcessUpdatedGesture(std::move(event));
  } else if (android_ui_gesture_target_) {
    android_ui_gesture_target_->DispatchWebInputEvent(std::move(event));
  }
}

device::mojom::VRPosePtr VrShell::VRPosePtrFromGvrPose(gvr::Mat4f head_mat) {
  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();

  pose->timestamp = base::Time::Now().ToJsTime();
  pose->orientation.emplace(4);

  gfx::Transform inv_transform(
      head_mat.m[0][0], head_mat.m[0][1], head_mat.m[0][2], head_mat.m[0][3],
      head_mat.m[1][0], head_mat.m[1][1], head_mat.m[1][2], head_mat.m[1][3],
      head_mat.m[2][0], head_mat.m[2][1], head_mat.m[2][2], head_mat.m[2][3],
      head_mat.m[3][0], head_mat.m[3][1], head_mat.m[3][2], head_mat.m[3][3]);

  gfx::Transform transform;
  if (inv_transform.GetInverse(&transform)) {
    gfx::DecomposedTransform decomposed_transform;
    gfx::DecomposeTransform(&decomposed_transform, transform);

    pose->orientation.value()[0] = decomposed_transform.quaternion[0];
    pose->orientation.value()[1] = decomposed_transform.quaternion[1];
    pose->orientation.value()[2] = decomposed_transform.quaternion[2];
    pose->orientation.value()[3] = decomposed_transform.quaternion[3];

    pose->position.emplace(3);
    pose->position.value()[0] = decomposed_transform.translate[0];
    pose->position.value()[1] = decomposed_transform.translate[1];
    pose->position.value()[2] = decomposed_transform.translate[2];
  }

  return pose;
}

device::mojom::VRDisplayInfoPtr VrShell::CreateVRDisplayInfo(
    gvr::GvrApi* gvr_api,
    gvr::Sizei compositor_size,
    uint32_t device_id) {
  TRACE_EVENT0("input", "GvrDevice::GetVRDevice");

  device::mojom::VRDisplayInfoPtr device = device::mojom::VRDisplayInfo::New();

  device->index = device_id;

  device->capabilities = device::mojom::VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  device->leftEye = device::mojom::VREyeParameters::New();
  device->rightEye = device::mojom::VREyeParameters::New();
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    device::mojom::VREyeParametersPtr& eye_params =
        (eye == GVR_LEFT_EYE) ? device->leftEye : device->rightEye;
    eye_params->fieldOfView = device::mojom::VRFieldOfView::New();
    eye_params->offset.resize(3);
    eye_params->renderWidth = compositor_size.width / 2;
    eye_params->renderHeight = compositor_size.height;

    gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
    gvr_buffer_viewports.GetBufferViewport(eye, &eye_viewport);
    gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
    eye_params->fieldOfView->upDegrees = eye_fov.top;
    eye_params->fieldOfView->downDegrees = eye_fov.bottom;
    eye_params->fieldOfView->leftDegrees = eye_fov.left;
    eye_params->fieldOfView->rightDegrees = eye_fov.right;

    gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
    eye_params->offset[0] = -eye_mat.m[0][3];
    eye_params->offset[1] = -eye_mat.m[1][3];
    eye_params->offset[2] = -eye_mat.m[2][3];
  }

  return device;
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& ui_web_contents,
           jlong content_window_android,
           jlong ui_window_android,
           jboolean for_web_vr,
           const base::android::JavaParamRef<jobject>& delegate,
           jlong gvr_api,
           jboolean reprojected_rendering) {
  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, reinterpret_cast<ui::WindowAndroid*>(content_window_android),
      content::WebContents::FromJavaWebContents(ui_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(ui_window_android), for_web_vr,
      VrShellDelegate::GetNativeVrShellDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering));
}

}  // namespace vr_shell
