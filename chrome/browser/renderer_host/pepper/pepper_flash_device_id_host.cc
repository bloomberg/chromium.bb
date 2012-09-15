// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_flash_device_id_host.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

using content::BrowserPpapiHost;
using content::BrowserThread;

namespace chrome {

namespace {

// The path to the file containing the DRM ID.
// It is mirrored from
//   chrome/browser/chromeos/system/drm_settings.cc
const char kDRMIdentifierFile[] = "Pepper DRM ID.0";

}  // namespace

// PepperFlashDeviceIDHost::Fetcher --------------------------------------------

PepperFlashDeviceIDHost::Fetcher::Fetcher(
    const base::WeakPtr<PepperFlashDeviceIDHost>& host)
    : host_(host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

PepperFlashDeviceIDHost::Fetcher::~Fetcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void PepperFlashDeviceIDHost::Fetcher::Start(BrowserPpapiHost* browser_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int process_id = 0;
  int view_id = 0;
  browser_host->GetRenderViewIDsForInstance(host_->pp_instance(),
                                            &process_id,
                                            &view_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PepperFlashDeviceIDHost::Fetcher::GetFilePathOnUIThread,
                 this, process_id, view_id));
}

void PepperFlashDeviceIDHost::Fetcher::GetFilePathOnUIThread(
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath path;
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (render_view_host) {
    content::BrowserContext* browser_context =
        render_view_host->GetProcess()->GetBrowserContext();
    // Don't use DRM IDs when incognito to prevent tracking.
    if (!browser_context->IsOffTheRecord())
      path = browser_context->GetPath().AppendASCII(kDRMIdentifierFile);
  }

  // Continue on the file thread (on failure/incognito, this will just send
  // the empty path which will forward the error).
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&PepperFlashDeviceIDHost::Fetcher::ReadDRMFileOnFileThread,
                 this, path));
}

void PepperFlashDeviceIDHost::Fetcher::ReadDRMFileOnFileThread(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string contents;
  if (!path.empty())
    file_util::ReadFileToString(path, &contents);

  // Give back to the resource host on the IO thread. On failure this will just
  // forward the empty string.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PepperFlashDeviceIDHost::Fetcher::ReplyOnIOThread,
                 this, contents));
}

void PepperFlashDeviceIDHost::Fetcher::ReplyOnIOThread(
    const std::string& contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (host_.get())  // Plugin or resource could have been deleted.
    host_->GotDRMContents(contents);
}

// PepperFlashDeviceIDHost -----------------------------------------------------

PepperFlashDeviceIDHost::PepperFlashDeviceIDHost(BrowserPpapiHost* host,
                                                 PP_Instance instance,
                                                 PP_Resource resource)
    : ppapi::host::ResourceHost(host->GetPpapiHost(), instance, resource),
      factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      browser_ppapi_host_(host) {
}

PepperFlashDeviceIDHost::~PepperFlashDeviceIDHost() {
}

int32_t PepperFlashDeviceIDHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperFlashDeviceIDHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(PpapiHostMsg_FlashDeviceID_GetDeviceID,
                                        OnHostMsgGetDeviceID)
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperFlashDeviceIDHost::OnHostMsgGetDeviceID(
    const ppapi::host::HostMessageContext* context) {
  if (fetcher_)
    return PP_ERROR_INPROGRESS;

  fetcher_ = new Fetcher(factory_.GetWeakPtr());
  fetcher_->Start(browser_ppapi_host_);
  reply_params_ = context->MakeReplyParams();
  return PP_OK_COMPLETIONPENDING;
}

void PepperFlashDeviceIDHost::GotDRMContents(const std::string& contents) {
  fetcher_ = NULL;
  reply_params_.set_result(contents.empty() ? PP_ERROR_FAILED : PP_OK);
  host()->SendReply(reply_params_,
                    PpapiPluginMsg_FlashDeviceID_GetDeviceIDReply(contents));
}

}  // namespace chrome
