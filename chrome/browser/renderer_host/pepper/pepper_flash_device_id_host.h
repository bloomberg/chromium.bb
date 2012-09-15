// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_DEVICE_ID_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_DEVICE_ID_HOST_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

class FilePath;

namespace content {
class BrowserPpapiHost;
}

namespace chrome {

class PepperFlashDeviceIDHost : public ppapi::host::ResourceHost {
 public:
  PepperFlashDeviceIDHost(content::BrowserPpapiHost* host,
                          PP_Instance instance,
                          PP_Resource resource);
  virtual ~PepperFlashDeviceIDHost();

  // ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

 private:
  // Helper class to manage the unfortunate number of thread transitions
  // required for this operation. We must first get the profile info from the
  // UI thread, and then actually read the file from the FILE thread before
  // returning to the IO thread to forward the result to the plugin.
  class Fetcher
      : public base::RefCountedThreadSafe<
            Fetcher,
            content::BrowserThread::DeleteOnIOThread> {
   public:
    explicit Fetcher(const base::WeakPtr<PepperFlashDeviceIDHost>& host);

    // Schedules the request operation. The host will be called back with
    // GotDRMContents. On failure, the contents will be empty.
    void Start(content::BrowserPpapiHost* browser_host);

   private:
    friend struct content::BrowserThread::DeleteOnThread<
        content::BrowserThread::IO>;
    friend class base::DeleteHelper<Fetcher>;

    ~Fetcher();

    // Called on the UI thread to get the file path corresponding to the
    // given render view. It will schedule the resulting file to be read on the
    // file thread. On error, it will pass the empty path to the file thread.
    void GetFilePathOnUIThread(int render_process_id,
                               int render_view_id);

    // Called on the file thread to read the contents of the file and to
    // forward it to the IO thread. The path will be empty on error (in
    // which case it will forward the empty string to the IO thread).
    void ReadDRMFileOnFileThread(const FilePath& path);

    // Called on the IO thread to call back into the device ID host with the
    // file contents, or the empty string on failure.
    void ReplyOnIOThread(const std::string& contents);

    // Access only on IO thread (this class is destroyed on the IO thread
    // to ensure that this is deleted properly, since weak ptrs aren't
    // threadsafe).
    base::WeakPtr<PepperFlashDeviceIDHost> host_;

    DISALLOW_COPY_AND_ASSIGN(Fetcher);
  };
  friend class Fetcher;

  // IPC message handler.
  int32_t OnHostMsgGetDeviceID(const ppapi::host::HostMessageContext* context);

  // Called by the fetcher when the DRM ID was retrieved, or the empty string
  // on error.
  void GotDRMContents(const std::string& contents);

  base::WeakPtrFactory<PepperFlashDeviceIDHost> factory_;

  content::BrowserPpapiHost* browser_ppapi_host_;

  // When a request is pending, the fetcher will be non-null, and the reply
  // params will have the appropriate routing info set for the reply.
  scoped_refptr<Fetcher> fetcher_;
  ppapi::proxy::ResourceMessageReplyParams reply_params_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashDeviceIDHost);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_DEVICE_ID_HOST_H_
