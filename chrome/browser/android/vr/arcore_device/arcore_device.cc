// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl_thread.h"
#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/vr_display_impl.h"
#include "ui/display/display.h"

using base::android::JavaRef;

namespace {
constexpr float kDegreesPerRadian = 180.0f / base::kPiFloat;
}  // namespace

namespace device {

namespace {

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(mojom::XRDeviceId device_id) {
  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->id = device_id;
  device->displayName = "ARCore VR Device";
  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = true;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = false;
  device->capabilities->can_provide_pass_through_images = true;
  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = nullptr;
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  left_eye->fieldOfView = mojom::VRFieldOfView::New();
  // TODO(lincolnfrog): get these values for real (see gvr device).
  uint width = 1080;
  uint height = 1795;
  double fov_x = 1437.387;
  double fov_y = 1438.074;
  // TODO(lincolnfrog): get real camera intrinsics.
  float horizontal_degrees = atan(width / (2.0 * fov_x)) * kDegreesPerRadian;
  float vertical_degrees = atan(height / (2.0 * fov_y)) * kDegreesPerRadian;
  left_eye->fieldOfView->leftDegrees = horizontal_degrees;
  left_eye->fieldOfView->rightDegrees = horizontal_degrees;
  left_eye->fieldOfView->upDegrees = vertical_degrees;
  left_eye->fieldOfView->downDegrees = vertical_degrees;
  left_eye->offset = {0.0f, 0.0f, 0.0f};
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  return device;
}

}  // namespace

ArCoreDevice::ArCoreDevice()
    : VRDeviceBase(mojom::XRDeviceId::ARCORE_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mailbox_bridge_(std::make_unique<vr::MailboxToSurfaceBridge>()),
      weak_ptr_factory_(this) {
  SetVRDisplayInfo(CreateVRDisplayInfo(GetId()));

  arcore_java_utils_ = std::make_unique<vr::ArCoreJavaUtils>(this);

  // TODO(https://crbug.com/836524) clean up usage of mailbox bridge
  // and extract the methods in this class that interact with ARCore API
  // into a separate class that implements the ArCore interface.
  mailbox_bridge_->CreateUnboundContextProvider(
      base::BindOnce(&ArCoreDevice::OnMailboxBridgeReady, GetWeakPtr()));
}

ArCoreDevice::~ArCoreDevice() {}

void ArCoreDevice::PauseTracking() {
  DCHECK(IsOnMainThread());

  if (is_paused_)
    return;

  is_paused_ = true;

  if (!is_arcore_gl_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::Pause, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr()));
}

void ArCoreDevice::ResumeTracking() {
  DCHECK(IsOnMainThread());

  if (!is_paused_)
    return;

  is_paused_ = false;

  // TODO(crbug.com/883046): ResumeTracking does not fire after ArCore has been
  // updated/installed or the update/installation was cancelled. Thus, we never
  // handle queued up session requests.
  if (on_request_arcore_install_or_update_result_callback_)
    std::move(on_request_arcore_install_or_update_result_callback_)
        .Run(!arcore_java_utils_->ShouldRequestInstallSupportedArCore());

  if (!is_arcore_gl_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::Resume, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr()));
}

void ArCoreDevice::OnMailboxBridgeReady() {
  DCHECK(IsOnMainThread());
  DCHECK(!arcore_gl_thread_);
  // MailboxToSurfaceBridge's destructor's call to DestroyContext must
  // happen on the GL thread, so transferring it to that thread is appropriate.
  // TODO(https://crbug.com/836553): use same GL thread as GVR.
  arcore_gl_thread_ = std::make_unique<ArCoreGlThread>(
      std::move(mailbox_bridge_),
      CreateMainThreadCallback(base::BindOnce(
          &ArCoreDevice::OnArCoreGlThreadInitialized, GetWeakPtr())));
  arcore_gl_thread_->Start();
}

void ArCoreDevice::OnArCoreGlThreadInitialized() {
  DCHECK(IsOnMainThread());

  is_arcore_gl_thread_initialized_ = true;

  if (pending_request_ar_module_callback_) {
    std::move(pending_request_ar_module_callback_).Run();
  }
}

void ArCoreDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK(IsOnMainThread());

  // If we are currently handling another request defer this request. All
  // deferred requests will be processed once handling is complete.
  deferred_request_session_callbacks_.push_back(std::move(callback));
  if (deferred_request_session_callbacks_.size() > 1) {
    return;
  }

  // TODO(https://crbug.com/849568): Instead of splitting the initialization
  // of this class between construction and RequestSession, perform all the
  // initialization at once on the first successful RequestSession call.
  if (!is_arcore_gl_thread_initialized_) {
    pending_request_ar_module_callback_ =
        base::BindOnce(&ArCoreDevice::RequestArModule, GetWeakPtr(),
                       options->render_process_id, options->render_frame_id,
                       options->has_user_activation);
    return;
  }

  RequestArModule(options->render_process_id, options->render_frame_id,
                  options->has_user_activation);
}

void ArCoreDevice::RequestArModule(int render_process_id,
                                   int render_frame_id,
                                   bool has_user_activation) {
  if (arcore_java_utils_->ShouldRequestInstallArModule()) {
    on_request_ar_module_result_callback_ =
        base::BindOnce(&ArCoreDevice::OnRequestArModuleResult, GetWeakPtr(),
                       render_process_id, render_frame_id, has_user_activation);
    arcore_java_utils_->RequestInstallArModule();
    return;
  }

  OnRequestArModuleResult(render_process_id, render_frame_id,
                          has_user_activation, true);
}

void ArCoreDevice::OnRequestArModuleResult(int render_process_id,
                                           int render_frame_id,
                                           bool has_user_activation,
                                           bool success) {
  if (!success) {
    CallDeferredRequestSessionCallbacks(/*success=*/false);
    return;
  }

  RequestArCoreInstallOrUpdate(render_process_id, render_frame_id,
                               has_user_activation);
}

void ArCoreDevice::RequestArCoreInstallOrUpdate(int render_process_id,
                                                int render_frame_id,
                                                bool has_user_activation) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);
  DCHECK(!on_request_arcore_install_or_update_result_callback_);

  if (arcore_java_utils_->ShouldRequestInstallSupportedArCore()) {
    // ARCore is not installed or requires an update. Store the callback to be
    // processed later once installation/update is complete or got cancelled.
    on_request_arcore_install_or_update_result_callback_ = base::BindOnce(
        &ArCoreDevice::OnRequestArCoreInstallOrUpdateResult, GetWeakPtr(),
        render_process_id, render_frame_id, has_user_activation);

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id, render_frame_id);
    DCHECK(render_frame_host);

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    DCHECK(web_contents);

    TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
    DCHECK(tab_android);

    base::android::ScopedJavaLocalRef<jobject> j_tab_android =
        tab_android->GetJavaObject();
    DCHECK(!j_tab_android.is_null());

    arcore_java_utils_->RequestInstallSupportedArCore(j_tab_android);
    return;
  }

  OnRequestArCoreInstallOrUpdateResult(render_process_id, render_frame_id,
                                       has_user_activation, true);
}

void ArCoreDevice::OnRequestInstallArModuleResult(bool success) {
  DCHECK(IsOnMainThread());

  if (on_request_ar_module_result_callback_) {
    std::move(on_request_ar_module_result_callback_).Run(success);
  }
}

void ArCoreDevice::OnRequestInstallSupportedArCoreCanceled() {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);
  DCHECK(on_request_arcore_install_or_update_result_callback_);

  std::move(on_request_arcore_install_or_update_result_callback_).Run(false);
}

void ArCoreDevice::CallDeferredRequestSessionCallbacks(bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);
  DCHECK(!deferred_request_session_callbacks_.empty());

  for (auto& deferred_callback : deferred_request_session_callbacks_) {
    mojom::XRSessionControllerPtr controller;
    mojom::XRSessionPtr session;
    if (success) {
      mojom::XRFrameDataProviderPtr data_provider;
      mojom::XREnvironmentIntegrationProviderPtr environment_provider;
      magic_window_sessions_.push_back(std::make_unique<VRDisplayImpl>(
          this, mojo::MakeRequest(&data_provider),
          mojo::MakeRequest(&environment_provider),
          mojo::MakeRequest(&controller)));

      session = mojom::XRSession::New();
      session->data_provider = data_provider.PassInterface();
      session->environment_provider = environment_provider.PassInterface();
      session->display_info = display_info_.Clone();
    }
    // We don't expect this call to alter deferred_request_session_callbacks_.
    // The call may request another session, which should be handled right here
    // in this loop as well.
    std::move(deferred_callback).Run(std::move(session), std::move(controller));
  }
  deferred_request_session_callbacks_.clear();
}

void ArCoreDevice::OnRequestArCoreInstallOrUpdateResult(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!success) {
    CallDeferredRequestSessionCallbacks(/*success=*/false);
    return;
  }

  // TODO(https://crbug.com/845792): Consider calling a method to ask for the
  // appropriate permissions.
  // ARCore sessions require camera permission.
  RequestCameraPermission(
      render_process_id, render_frame_id, has_user_activation,
      base::BindOnce(&ArCoreDevice::OnRequestCameraPermissionComplete,
                     GetWeakPtr()));
}

void ArCoreDevice::OnRequestCameraPermissionComplete(bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!success) {
    CallDeferredRequestSessionCallbacks(/*success=*/false);
    return;
  }

  // By this point ARCore has already been set up, so continue handling request.
  RequestArCoreGlInitialization();
}

bool ArCoreDevice::ShouldPauseTrackingWhenFrameDataRestricted() {
  return true;
}

void ArCoreDevice::OnMagicWindowFrameDataRequest(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsOnMainThread());
  // We should not be able to reach this point if we are not initialized.
  DCHECK(is_arcore_gl_thread_initialized_);

  if (is_paused_) {
    std::move(callback).Run(nullptr);
    return;
  }

  // TODO(https://crbug.com/836496) This current implementation does not handle
  // multiple sessions well. There should be a better way to handle this than
  // taking the max of all sessions.
  gfx::Size max_size(0, 0);
  display::Display::Rotation rotation;
  for (auto& session : magic_window_sessions_) {
    max_size.SetToMax(session->sessionFrameSize());
    // We have to pick a rotation so just go with the last one.
    rotation = session->sessionRotation();
  }

  if (max_size.IsEmpty()) {
    DLOG(ERROR) << "No valid AR frame size provided!";
    std::move(callback).Run(nullptr);
    return;
  }

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::ProduceFrame, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
      max_size, rotation, CreateMainThreadCallback(std::move(callback))));
}

void ArCoreDevice::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  DCHECK(IsOnMainThread());

  PostTaskToGlThread(base::BindOnce(
      &ArCoreGl::RequestHitTest, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
      std::move(ray), CreateMainThreadCallback(std::move(callback))));
}

void ArCoreDevice::PostTaskToGlThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  arcore_gl_thread_->GetArCoreGl()->GetGlThreadTaskRunner()->PostTask(
      FROM_HERE, std::move(task));
}

bool ArCoreDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

void ArCoreDevice::RequestCameraPermission(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  // The RFH may have been destroyed by the time the request is processed.
  DCHECK(rfh);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  PermissionManager* permission_manager = PermissionManager::Get(profile);

  permission_manager->RequestPermission(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, rfh, web_contents->GetURL(),
      has_user_activation,
      base::BindRepeating(&ArCoreDevice::OnRequestCameraPermissionResult,
                          GetWeakPtr(), web_contents, base::Passed(&callback)));
}

void ArCoreDevice::OnRequestCameraPermissionResult(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    ContentSetting content_setting) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  // If the camera permission is not allowed, abort the request.
  if (content_setting != CONTENT_SETTING_ALLOW) {
    std::move(callback).Run(false);
    return;
  }

  // Even if the content setting stated that the camera access is allowed,
  // the Android camera permission might still need to be requested, so check
  // if the OS level permission infobar should be shown.
  std::vector<ContentSettingsType> content_settings_types;
  content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  ShowPermissionInfoBarState show_permission_info_bar_state =
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfoBar(
          web_contents, content_settings_types);
  switch (show_permission_info_bar_state) {
    case ShowPermissionInfoBarState::NO_NEED_TO_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(true);
      return;
    case ShowPermissionInfoBarState::SHOW_PERMISSION_INFOBAR:
      // Show the Android camera permission info bar.
      PermissionUpdateInfoBarDelegate::Create(
          web_contents, content_settings_types,
          base::BindOnce(&ArCoreDevice::OnRequestAndroidCameraPermissionResult,
                         GetWeakPtr(), base::Passed(&callback)));
      return;
    case ShowPermissionInfoBarState::CANNOT_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(false);
      return;
  }

  NOTREACHED() << "Unknown show permission infobar state.";
}

void ArCoreDevice::RequestArCoreGlInitialization() {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!is_arcore_gl_initialized_) {
    PostTaskToGlThread(base::BindOnce(
        &ArCoreGl::Initialize, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr(),
        CreateMainThreadCallback(base::BindOnce(
            &ArCoreDevice::OnArCoreGlInitializationComplete, GetWeakPtr()))));
    return;
  }

  OnArCoreGlInitializationComplete(true);
}

void ArCoreDevice::OnArCoreGlInitializationComplete(bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!success) {
    CallDeferredRequestSessionCallbacks(/*success=*/false);
    return;
  }

  is_arcore_gl_initialized_ = true;

  if (!is_paused_) {
    PostTaskToGlThread(base::BindOnce(
        &ArCoreGl::Resume, arcore_gl_thread_->GetArCoreGl()->GetWeakPtr()));
  }

  CallDeferredRequestSessionCallbacks(/*success=*/true);
}

void ArCoreDevice::OnRequestAndroidCameraPermissionResult(
    base::OnceCallback<void(bool)> callback,
    bool was_android_camera_permission_granted) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  std::move(callback).Run(was_android_camera_permission_granted);
}

}  // namespace device
