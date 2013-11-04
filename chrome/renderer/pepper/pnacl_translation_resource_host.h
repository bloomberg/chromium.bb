// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PNACL_TRANSLATION_RESOURCE_HOST_H_
#define CHROME_RENDERER_PEPPER_PNACL_TRANSLATION_RESOURCE_HOST_H_

#include <map>

#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace nacl {
struct PnaclCacheInfo;
}

// A class to keep track of requests made to the browser for resources that the
// PNaCl translator needs (e.g. descriptors for the translator nexes, temp
// files, and cached translations).

// "Resource" might not be the best name for the various things that pnacl
// needs from the browser since "Resource" is a Pepper thing...
class PnaclTranslationResourceHost : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit PnaclTranslationResourceHost(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);
  void RequestNexeFd(int render_view_id,
                     PP_Instance instance,
                     const nacl::PnaclCacheInfo& cache_info,
                     PP_Bool* is_hit,
                     PP_FileHandle* file_handle,
                     scoped_refptr<ppapi::TrackedCallback> callback);
  void ReportTranslationFinished(PP_Instance instance, PP_Bool success);

 protected:
  virtual ~PnaclTranslationResourceHost();

 private:
  class CacheRequestInfo {
   public:
    CacheRequestInfo(PP_Bool* hit,
                     PP_FileHandle* handle,
                     scoped_refptr<ppapi::TrackedCallback> cb);

    ~CacheRequestInfo();

    PP_Bool* is_hit;
    PP_FileHandle* file_handle;
    scoped_refptr<ppapi::TrackedCallback> callback;
  };

  // Maps the instance with an outstanding cache request to the info
  // about that request.
  typedef std::map<PP_Instance, CacheRequestInfo> CacheRequestInfoMap;
  // IPC::ChannelProxy::MessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  void SendRequestNexeFd(int render_view_id,
                         PP_Instance instance,
                         const nacl::PnaclCacheInfo& cache_info,
                         PP_Bool* is_hit,
                         PP_FileHandle* file_handle,
                         scoped_refptr<ppapi::TrackedCallback> callback);
  void SendReportTranslationFinished(PP_Instance instance,
                                     PP_Bool success);
  void OnNexeTempFileReply(PP_Instance instance,
                           bool is_hit,
                           IPC::PlatformFileForTransit file);
  void CleanupCacheRequests();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Should be accessed on the io thread.
  IPC::Channel* channel_;
  CacheRequestInfoMap pending_cache_requests_;
  DISALLOW_COPY_AND_ASSIGN(PnaclTranslationResourceHost);
};

#endif  // CHROME_RENDERER_PEPPER_PNACL_TRANSLATION_RESOURCE_HOST_H_
