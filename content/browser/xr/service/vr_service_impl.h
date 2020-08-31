// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_
#define CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "content/browser/xr/metrics/session_metrics_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/xr_consent_helper.h"
#include "content/public/browser/xr_consent_prompt_level.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace content {

class XRRuntimeManagerImpl;
class XRRuntimeManagerTest;
class BrowserXRRuntimeImpl;

// Browser process implementation of the VRService mojo interface. Instantiated
// through Mojo once the user loads a page containing WebXR.
class CONTENT_EXPORT VRServiceImpl : public device::mojom::VRService,
                                     content::WebContentsObserver {
 public:
  explicit VRServiceImpl(content::RenderFrameHost* render_frame_host);

  // Constructor for tests.
  explicit VRServiceImpl(util::PassKey<XRRuntimeManagerTest>);

  ~VRServiceImpl() override;

  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<device::mojom::VRService> receiver);

  // device::mojom::VRService implementation
  void SetClient(mojo::PendingRemote<device::mojom::VRServiceClient>
                     service_client) override;
  void RequestSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::RequestSessionCallback callback) override;
  void SupportsSession(
      device::mojom::XRSessionOptionsPtr options,
      device::mojom::VRService::SupportsSessionCallback callback) override;
  void ExitPresent(ExitPresentCallback on_exited) override;
  void SetFramesThrottled(bool throttled) override;

  void InitializationComplete();

  // Called when inline session gets disconnected. |session_id| is the value
  // returned by |magic_window_controllers_| when adding session controller to
  // it.
  void OnInlineSessionDisconnected(mojo::RemoteSetElementId session_id);

  // Notifications/calls from BrowserXRRuntimeImpl:
  void OnExitPresent();
  void OnVisibilityStateChanged(
      device::mojom::XRVisibilityState visibility_state);
  void OnDisplayInfoChanged();
  void RuntimesChanged();

  base::WeakPtr<VRServiceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  content::WebContents* GetWebContents();

 private:
  struct SessionRequestData {
    device::mojom::XRSessionOptionsPtr options;
    device::mojom::VRService::RequestSessionCallback callback;
    std::set<device::mojom::XRSessionFeature> enabled_features;
    device::mojom::XRDeviceId runtime_id;

    SessionRequestData(
        device::mojom::XRSessionOptionsPtr options,
        device::mojom::VRService::RequestSessionCallback callback,
        std::set<device::mojom::XRSessionFeature> enabled_features,
        device::mojom::XRDeviceId runtime_id);
    ~SessionRequestData();
    SessionRequestData(SessionRequestData&&);

   private:
    SessionRequestData(const SessionRequestData&) = delete;
    SessionRequestData& operator=(const SessionRequestData&) = delete;
  };

  // content::WebContentsObserver implementation
  void OnWebContentsFocused(content::RenderWidgetHost* host) override;
  void OnWebContentsLostFocus(content::RenderWidgetHost* host) override;
  void RenderFrameDeleted(content::RenderFrameHost* host) override;

  void OnWebContentsFocusChanged(content::RenderWidgetHost* host, bool focused);

  void ResolvePendingRequests();

  // Returns currently active instance of SessionMetricsHelper from WebContents.
  // If the instance is not present on WebContents, it will be created with the
  // assumption that we are not already in VR.
  SessionMetricsHelper* GetSessionMetricsHelper();

  bool InternalSupportsSession(device::mojom::XRSessionOptions* options);

  bool IsConsentGrantedForDevice(device::mojom::XRDeviceId device_id,
                                 content::XrConsentPromptLevel consent_level);
  void AddConsentGrantedDevice(device::mojom::XRDeviceId device_id,
                               content::XrConsentPromptLevel consent_level);

  // The following steps are ordered in the general flow for "RequestSession"
  // If the WebXrPermissionsAPI is enabled ShowConsentPrompt will result in a
  // call to OnPermissionResult which feeds into OnConsentResult.
  // If ShowConsentPrompt determines that no consent/permission is needed (or
  // has already been granted), then it will directly call
  // EnsureRuntimeInstalled. DoRequestSession will continue with OnInline or
  // OnImmersive SessionCreated depending on the type of session created.
  void ShowConsentPrompt(SessionRequestData request,
                         BrowserXRRuntimeImpl* runtime);

  void OnConsentResult(SessionRequestData request,
                       content::XrConsentPromptLevel consent_level,
                       bool is_consent_granted);
  void OnPermissionResult(SessionRequestData request,
                          content::XrConsentPromptLevel consent_level,
                          blink::mojom::PermissionStatus permission_status);

  void EnsureRuntimeInstalled(SessionRequestData request,
                              BrowserXRRuntimeImpl* runtime);
  void OnInstallResult(SessionRequestData request_data, bool install_succeeded);

  void DoRequestSession(SessionRequestData request);

  void OnInlineSessionCreated(
      SessionRequestData request,
      device::mojom::XRSessionPtr session,
      mojo::PendingRemote<device::mojom::XRSessionController> controller);
  void OnImmersiveSessionCreated(SessionRequestData request,
                                 device::mojom::XRSessionPtr session);
  void OnSessionCreated(
      SessionRequestData request,
      device::mojom::XRSessionPtr session,
      mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
          session_metrics_recorder);

  scoped_refptr<XRRuntimeManagerImpl> runtime_manager_;
  mojo::RemoteSet<device::mojom::XRSessionClient> session_clients_;
  mojo::Remote<device::mojom::VRServiceClient> service_client_;
  content::RenderFrameHost* render_frame_host_;
  mojo::SelfOwnedReceiverRef<device::mojom::VRService> receiver_;
  mojo::RemoteSet<device::mojom::XRSessionController> magic_window_controllers_;
  device::mojom::XRVisibilityState visibility_state_ =
      device::mojom::XRVisibilityState::VISIBLE;

  // List of callbacks to run when initialization is completed.
  std::vector<base::OnceCallback<void()>> pending_requests_;

  bool initialization_complete_ = false;
  bool in_focused_frame_ = false;
  bool frames_throttled_ = false;

  std::map<device::mojom::XRDeviceId, content::XrConsentPromptLevel>
      consent_granted_devices_;

  base::WeakPtrFactory<VRServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VRServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_SERVICE_VR_SERVICE_IMPL_H_
