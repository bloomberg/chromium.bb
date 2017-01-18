// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_shell.h"

#include <android/native_window_jni.h>

#include "base/metrics/histogram_macros.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/vr_compositor.h"
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
#include "content/public/common/referrer.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "jni/VrShellImpl_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace vr_shell {

namespace {
vr_shell::VrShell* g_instance;

static const char kVrShellUIURL[] = "chrome://vr-shell-ui";

class GLThread : public base::Thread {
 public:
  GLThread(const base::WeakPtr<VrShell>& weak_vr_shell,
           const base::WeakPtr<VrInputManager>& content_input_manager,
           const base::WeakPtr<VrInputManager>& ui_input_manager,
           scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
           gvr_context* gvr_api,
           bool initially_web_vr,
           bool reprojected_rendering)
      : base::Thread("VrShellGL"),
        weak_vr_shell_(weak_vr_shell),
        content_input_manager_(content_input_manager),
        ui_input_manager_(ui_input_manager),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        gvr_api_(gvr_api),
        initially_web_vr_(initially_web_vr),
        reprojected_rendering_(reprojected_rendering) {}

  ~GLThread() override {
    Stop();
  }
  base::WeakPtr<VrShellGl> GetVrShellGl() { return weak_vr_shell_gl_; }
  VrShellGl* GetVrShellGlUnsafe() { return vr_shell_gl_.get(); }

 protected:
  void Init() override {
    vr_shell_gl_.reset(new VrShellGl(std::move(weak_vr_shell_),
                                     std::move(content_input_manager_),
                                     std::move(ui_input_manager_),
                                     std::move(main_thread_task_runner_),
                                     gvr_api_,
                                     initially_web_vr_,
                                     reprojected_rendering_));
    weak_vr_shell_gl_ = vr_shell_gl_->GetWeakPtr();
    vr_shell_gl_->Initialize();
  }
  void CleanUp() override {
    vr_shell_gl_.reset();
  }

 private:
  // Created on GL thread.
  std::unique_ptr<VrShellGl> vr_shell_gl_;
  base::WeakPtr<VrShellGl> weak_vr_shell_gl_;

  base::WeakPtr<VrShell> weak_vr_shell_;
  base::WeakPtr<VrInputManager> content_input_manager_;
  base::WeakPtr<VrInputManager> ui_input_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  gvr_context* gvr_api_;
  bool initially_web_vr_;
  bool reprojected_rendering_;
};

}  // namespace

VrShell::VrShell(JNIEnv* env,
                 jobject obj,
                 content::WebContents* main_contents,
                 ui::WindowAndroid* content_window,
                 content::WebContents* ui_contents,
                 ui::WindowAndroid* ui_window,
                 bool for_web_vr,
                 VrShellDelegate* delegate,
                 gvr_context* gvr_api,
                 bool reprojected_rendering)
    : WebContentsObserver(ui_contents),
      main_contents_(main_contents),
      content_compositor_(new VrCompositor(content_window, false)),
      ui_contents_(ui_contents),
      ui_compositor_(new VrCompositor(ui_window, true)),
      delegate_(delegate),
      metrics_helper_(new VrMetricsHelper(main_contents_)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reprojected_rendering_(reprojected_rendering),
      weak_ptr_factory_(this) {
  DCHECK(g_instance == nullptr);
  g_instance = this;
  j_vr_shell_.Reset(env, obj);

  content_input_manager_.reset(new VrInputManager(main_contents_));
  ui_input_manager_.reset(new VrInputManager(ui_contents_));

  content_compositor_->SetLayer(main_contents_);
  ui_compositor_->SetLayer(ui_contents_);

  gl_thread_.reset(new GLThread(weak_ptr_factory_.GetWeakPtr(),
                                content_input_manager_->GetWeakPtr(),
                                ui_input_manager_->GetWeakPtr(),
                                main_thread_task_runner_,
                                gvr_api,
                                for_web_vr,
                                reprojected_rendering_));

  base::Thread::Options options(base::MessageLoop::TYPE_DEFAULT, 0);
  options.priority = base::ThreadPriority::DISPLAY;
  gl_thread_->StartWithOptions(options);

  if (for_web_vr)
    metrics_helper_->SetWebVREnabled(true);
  html_interface_.reset(new UiInterface(
      for_web_vr ? UiInterface::Mode::WEB_VR : UiInterface::Mode::STANDARD,
      main_contents_->IsFullscreen()));
  vr_web_contents_observer_.reset(new VrWebContentsObserver(
      main_contents, html_interface_.get(), this));

  SetIsInVR(true);
}

void VrShell::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
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
  delegate_->RemoveDelegate();
  g_instance = nullptr;
}

void VrShell::PostToGlThreadWhenReady(const base::Closure& task) {
  // TODO(mthiesse): Remove this blocking wait. Queue up events if thread isn't
  // finished starting?
  gl_thread_->WaitUntilThreadStarted();
  gl_thread_->task_runner()->PostTask(FROM_HERE, task);
}

void VrShell::OnTriggerEvent(JNIEnv* env,
                             const JavaParamRef<jobject>& obj) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  thread->task_runner()->PostTask(FROM_HERE,
                                  base::Bind(&VrShellGl::OnTriggerEvent,
                                             thread->GetVrShellGl()));
}

void VrShell::OnPause(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VrShellGl::OnPause, thread->GetVrShellGl()));

  // exit vr session
  metrics_helper_->SetVRActive(false);
  SetIsInVR(false);
}

void VrShell::OnResume(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VrShellGl::OnResume, thread->GetVrShellGl()));

  metrics_helper_->SetVRActive(true);
  SetIsInVR(true);
}

void VrShell::SetSurface(JNIEnv* env,
                         const JavaParamRef<jobject>& obj,
                         const JavaParamRef<jobject>& surface) {
  CHECK(!reprojected_rendering_);
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  gfx::AcceleratedWidget window =
      ANativeWindow_fromSurface(base::android::AttachCurrentThread(), surface);
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::InitializeGl,
                                     thread->GetVrShellGl(),
                                     base::Unretained(window)));
}

void VrShell::SetIsInVR(bool is_in_vr) {
  main_contents_->GetRenderWidgetHostView()->SetIsInVR(is_in_vr);
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
  html_interface_->SetURL(main_contents_->GetVisibleURL());
  html_interface_->SetLoading(main_contents_->IsLoading());
  html_interface_->OnDomContentsLoaded();
}

void VrShell::SetWebVrMode(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool enabled) {
  metrics_helper_->SetWebVREnabled(enabled);
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::SetWebVrMode, thread->GetVrShellGl(), enabled));
  if (enabled) {
    html_interface_->SetMode(UiInterface::Mode::WEB_VR);
  } else {
    html_interface_->SetMode(UiInterface::Mode::STANDARD);
  }
}

void VrShell::OnLoadProgressChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    double progress) {
  html_interface_->SetLoadProgress(progress);
}

void VrShell::SetGvrPoseForWebVr(const gvr::Mat4f& pose, uint32_t pose_num) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  if (thread->IsRunning()) {
    thread->task_runner()->PostTask(
        FROM_HERE, base::Bind(&VrShellGl::SetGvrPoseForWebVr,
                              thread->GetVrShellGl(), pose, pose_num));
  }
}

void VrShell::SetWebVRRenderSurfaceSize(int width, int height) {
  // TODO(klausw,crbug.com/655722): Change the GVR render size and set the WebVR
  // render surface size.
}

gvr::Sizei VrShell::GetWebVRCompositorSurfaceSize() {
  const gfx::Size& size = content_compositor_->GetWindowBounds();
  return {size.width(), size.height()};
}

void VrShell::SetWebVRSecureOrigin(bool secure_origin) {
  // TODO(cjgrant): Align this state with the logic that drives the omnibox.
  html_interface_->SetWebVRSecureOrigin(secure_origin);
}

void VrShell::SubmitWebVRFrame() {}

void VrShell::UpdateWebVRTextureBounds(const gvr::Rectf& left_bounds,
                                       const gvr::Rectf& right_bounds) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UpdateWebVRTextureBounds,
                                     thread->GetVrShellGl(), left_bounds,
                                     right_bounds));
}

// TODO(mthiesse): Do not expose GVR API outside of GL thread.
// It's not thread-safe.
gvr::GvrApi* VrShell::gvr_api() {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  if (thread->GetVrShellGlUnsafe()) {
    return thread->GetVrShellGlUnsafe()->gvr_api();
  }
  CHECK(false);
  return nullptr;
}

void VrShell::SurfacesChanged(jobject content_surface, jobject ui_surface) {
  content_compositor_->SurfaceChanged(content_surface);
  ui_compositor_->SurfaceChanged(ui_surface);
}

void VrShell::GvrDelegateReady() {
  delegate_->SetDelegate(this);
}

void VrShell::AppButtonPressed() {
#if defined(ENABLE_VR_SHELL)
  html_interface_->SetMenuMode(!html_interface_->GetMenuMode());

  // TODO(mthiesse): The page is no longer visible when in menu mode. We
  // should unfocus or otherwise let it know it's hidden.
  if (html_interface_->GetMode() == UiInterface::Mode::WEB_VR) {
    if (delegate_->device_provider()) {
      if (html_interface_->GetMenuMode()) {
        delegate_->device_provider()->OnDisplayBlur();
      } else {
        delegate_->device_provider()->OnDisplayFocus();
      }
    }
  }
#endif
}

void VrShell::ContentPhysicalBoundsChanged(JNIEnv* env,
                                           const JavaParamRef<jobject>& object,
                                           jint width, jint height,
                                           jfloat dpr) {
  TRACE_EVENT0("gpu", "VrShell::ContentPhysicalBoundsChanged");
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::ContentPhysicalBoundsChanged,
                                     thread->GetVrShellGl(), width, height));
  content_compositor_->SetWindowBounds(gfx::Size(width, height));
}

void VrShell::UIPhysicalBoundsChanged(JNIEnv* env,
                                      const JavaParamRef<jobject>& object,
                                      jint width, jint height, jfloat dpr) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UIPhysicalBoundsChanged,
                                     thread->GetVrShellGl(), width, height));
  ui_compositor_->SetWindowBounds(gfx::Size(width, height));
}

UiInterface* VrShell::GetUiInterface() {
  return html_interface_.get();
}

void VrShell::UpdateScene(const base::ListValue* args) {
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(base::Bind(&VrShellGl::UpdateScene,
                                     thread->GetVrShellGl(),
                                     base::Passed(args->CreateDeepCopy())));
}

void VrShell::DoUiAction(const UiAction action) {
  content::NavigationController& controller = main_contents_->GetController();
  switch (action) {
    case HISTORY_BACK:
      if (main_contents_->IsFullscreen()) {
        main_contents_->ExitFullscreen(false);
      } else if (controller.CanGoBack()) {
        controller.GoBack();
      }
      break;
    case HISTORY_FORWARD:
      if (controller.CanGoForward())
        controller.GoForward();
      break;
    case RELOAD:
      controller.Reload(content::ReloadType::NORMAL, false);
      break;
#if defined(ENABLE_VR_SHELL_UI_DEV)
    case RELOAD_UI:
      ui_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
      html_interface_.reset(new UiInterface(UiInterface::Mode::STANDARD,
                                            main_contents_->IsFullscreen()));
      vr_web_contents_observer_->SetUiInterface(html_interface_.get());
      break;
#endif
    case ZOOM_OUT:  // Not handled yet.
    case ZOOM_IN:  // Not handled yet.
      break;
    default:
      NOTREACHED();
  }
}

void VrShell::RenderViewHostChanged(content::RenderViewHost* old_host,
                                    content::RenderViewHost* new_host) {
  new_host->GetWidget()->GetView()->SetBackgroundColor(SK_ColorTRANSPARENT);
}

void VrShell::MainFrameWasResized(bool width_changed) {
  display::Display display = display::Screen::GetScreen()
      ->GetDisplayNearestWindow(ui_contents_->GetNativeView());
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::UIBoundsChanged, thread->GetVrShellGl(),
                 display.size().width(), display.size().height()));
}

void VrShell::ContentFrameWasResized(bool width_changed) {
  display::Display display = display::Screen::GetScreen()
      ->GetDisplayNearestWindow(main_contents_->GetNativeView());
  GLThread* thread = static_cast<GLThread*>(gl_thread_.get());
  PostToGlThreadWhenReady(
      base::Bind(&VrShellGl::ContentBoundsChanged, thread->GetVrShellGl(),
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
  content_input_manager_.reset();
  // TODO(mthiesse): Handle web contents being hidden.
  ForceExitVr();
}

void VrShell::ForceExitVr() {
  delegate_->ForceExitVr();
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

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& content_web_contents,
           jlong content_window_android,
           const JavaParamRef<jobject>& ui_web_contents,
           jlong ui_window_android, jboolean for_web_vr,
           const base::android::JavaParamRef<jobject>& delegate,
           jlong gvr_api, jboolean reprojected_rendering) {
  return reinterpret_cast<intptr_t>(new VrShell(
      env, obj, content::WebContents::FromJavaWebContents(content_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(content_window_android),
      content::WebContents::FromJavaWebContents(ui_web_contents),
      reinterpret_cast<ui::WindowAndroid*>(ui_window_android),
      for_web_vr, VrShellDelegate::GetNativeDelegate(env, delegate),
      reinterpret_cast<gvr_context*>(gvr_api), reprojected_rendering));
}

}  // namespace vr_shell
