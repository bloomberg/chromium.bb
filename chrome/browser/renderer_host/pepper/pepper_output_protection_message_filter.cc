// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_output_protection_message_filter.h"

#include "build/build_config.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "chromeos/display/output_configurator.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#endif

namespace chrome {

namespace {

#if defined(OS_CHROMEOS)
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_NONE),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_UNKNOWN),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_INTERNAL),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_VGA),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_HDMI),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_DVI),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_DISPLAYPORT),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK) ==
    static_cast<int>(chromeos::OUTPUT_TYPE_NETWORK),
    PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE) ==
    static_cast<int>(chromeos::OUTPUT_PROTECTION_METHOD_NONE),
    PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE);
COMPILE_ASSERT(
    static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP) ==
    static_cast<int>(chromeos::OUTPUT_PROTECTION_METHOD_HDCP),
    PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP);

bool GetCurrentDisplayId(content::RenderViewHost* rvh, int64* display_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::RenderWidgetHostView* view = rvh->GetView();
  if (!view)
    return false;
  gfx::NativeView native_view = view->GetNativeView();
  gfx::Screen* screen = gfx::Screen::GetScreenFor(native_view);
  if (!screen)
    return false;
  gfx::Display display = screen->GetDisplayNearestWindow(native_view);
  *display_id = display.id();
  return true;
}
#endif

}  // namespace

#if defined(OS_CHROMEOS)
// Output protection delegate. All methods except constructor should be
// invoked in UI thread.
class PepperOutputProtectionMessageFilter::Delegate
    : public aura::WindowObserver {
 public:
  Delegate(int render_process_id, int render_view_id);
  virtual ~Delegate();

  // aura::WindowObserver overrides.
  virtual void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) OVERRIDE;

  int32_t OnQueryStatus(uint32_t* link_mask, uint32_t* protection_mask);
  int32_t OnEnableProtection(uint32_t desired_method_mask);

 private:
  chromeos::OutputConfigurator::OutputProtectionClientId GetClientId();

  // Used to lookup the WebContents associated with this PP_Instance.
  int render_process_id_;
  int render_view_id_;

  chromeos::OutputConfigurator::OutputProtectionClientId client_id_;
  // The display id which the renderer currently uses.
  int64 display_id_;
  // The last desired method mask. Will enable this mask on new display if
  // renderer changes display.
  uint32_t desired_method_mask_;
};

PepperOutputProtectionMessageFilter::Delegate::Delegate(int render_process_id,
                                                        int render_view_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      client_id_(chromeos::OutputConfigurator::kInvalidClientId),
      display_id_(0) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

}

PepperOutputProtectionMessageFilter::Delegate::~Delegate() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  configurator->UnregisterOutputProtectionClient(client_id_);

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id_, render_view_id_);
  if (rvh) {
    content::RenderWidgetHostView* view = rvh->GetView();
    if (view) {
      gfx::NativeView native_view = view->GetNativeView();
      native_view->RemoveObserver(this);
    }
  }
}

chromeos::OutputConfigurator::OutputProtectionClientId
PepperOutputProtectionMessageFilter::Delegate::GetClientId() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (client_id_ == chromeos::OutputConfigurator::kInvalidClientId) {
    content::RenderViewHost* rvh =
        content::RenderViewHost::FromID(render_process_id_, render_view_id_);
    if (!GetCurrentDisplayId(rvh, &display_id_))
      return chromeos::OutputConfigurator::kInvalidClientId;
    content::RenderWidgetHostView* view = rvh->GetView();
    if (!view)
      return chromeos::OutputConfigurator::kInvalidClientId;
    gfx::NativeView native_view = view->GetNativeView();
    if (!view)
      return chromeos::OutputConfigurator::kInvalidClientId;
    native_view->AddObserver(this);

    chromeos::OutputConfigurator* configurator =
        ash::Shell::GetInstance()->output_configurator();
    client_id_ = configurator->RegisterOutputProtectionClient();
  }
  return client_id_;
}

int32_t PepperOutputProtectionMessageFilter::Delegate::OnQueryStatus(
    uint32_t* link_mask, uint32_t* protection_mask) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id_, render_view_id_);
  if (!rvh) {
    LOG(WARNING) << "RenderViewHost is not alive.";
    return PP_ERROR_FAILED;
  }

  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  bool result = configurator->QueryOutputProtectionStatus(
      GetClientId(), display_id_, link_mask, protection_mask);

  // If we successfully retrieved the device level status, check for capturers.
  if (result) {
    if (content::WebContents::FromRenderViewHost(rvh)->GetCapturerCount() > 0)
      *link_mask |= chromeos::OUTPUT_TYPE_NETWORK;
  }

  return result ? PP_OK : PP_ERROR_FAILED;
}

int32_t PepperOutputProtectionMessageFilter::Delegate::OnEnableProtection(
    uint32_t desired_method_mask) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  chromeos::OutputConfigurator* configurator =
      ash::Shell::GetInstance()->output_configurator();
  bool result = configurator->EnableOutputProtection(
      GetClientId(), display_id_, desired_method_mask);
  desired_method_mask_ = desired_method_mask;
  return result ? PP_OK : PP_ERROR_FAILED;
}

void PepperOutputProtectionMessageFilter::Delegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(render_process_id_, render_view_id_);
  if (!rvh) {
    LOG(WARNING) << "RenderViewHost is not alive.";
    return;
  }

  int64 new_display_id = 0;
  if (!GetCurrentDisplayId(rvh, &new_display_id))
    return;
  if (display_id_ == new_display_id)
    return;

  if (desired_method_mask_ != chromeos::OUTPUT_PROTECTION_METHOD_NONE) {
    // Display changed and should enable output protections on new display.
    chromeos::OutputConfigurator* configurator =
        ash::Shell::GetInstance()->output_configurator();
    configurator->EnableOutputProtection(GetClientId(), new_display_id,
                                         desired_method_mask_);
    configurator->EnableOutputProtection(
        GetClientId(),
        display_id_,
        chromeos::OUTPUT_PROTECTION_METHOD_NONE);
  }
  display_id_ = new_display_id;
}
#endif

PepperOutputProtectionMessageFilter::PepperOutputProtectionMessageFilter(
    content::BrowserPpapiHost* host,
    PP_Instance instance) {
#if defined(OS_CHROMEOS)
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  int render_process_id = 0;
  int render_view_id = 0;
  host->GetRenderViewIDsForInstance(
      instance, &render_process_id, &render_view_id);
  delegate_ = new Delegate(render_process_id, render_view_id);
#else
  NOTIMPLEMENTED();
#endif
}

PepperOutputProtectionMessageFilter::~PepperOutputProtectionMessageFilter() {
#if defined(OS_CHROMEOS)
  content::BrowserThread::DeleteSoon(content::BrowserThread::UI, FROM_HERE,
                                     delegate_);
  delegate_ = NULL;
#endif
}

scoped_refptr<base::TaskRunner>
PepperOutputProtectionMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperOutputProtectionMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperOutputProtectionMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_OutputProtection_QueryStatus,
        OnQueryStatus);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_OutputProtection_EnableProtection,
        OnEnableProtection);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperOutputProtectionMessageFilter::OnQueryStatus(
    ppapi::host::HostMessageContext* context) {
#if defined(OS_CHROMEOS)
  uint32_t link_mask = 0, protection_mask = 0;
  int32_t result = delegate_->OnQueryStatus(&link_mask, &protection_mask);

  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  reply_context.params.set_result(result);
  SendReply(
      reply_context,
      PpapiPluginMsg_OutputProtection_QueryStatusReply(
          link_mask, protection_mask));
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

int32_t PepperOutputProtectionMessageFilter::OnEnableProtection(
    ppapi::host::HostMessageContext* context,
    uint32_t desired_method_mask) {
#if defined(OS_CHROMEOS)
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  int32_t result = delegate_->OnEnableProtection(desired_method_mask);
  reply_context.params.set_result(result);
  SendReply(
      reply_context,
      PpapiPluginMsg_OutputProtection_EnableProtectionReply());
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

}  // namespace chrome

