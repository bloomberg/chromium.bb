// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_
#define CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_
#pragma once

#include "build/build_config.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "ipc/ipc_channel.h"
#include "chrome/service/service_child_process_host.h"
#include "printing/pdf_render_settings.h"

class ScopedTempDir;

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace printing {
class Emf;
struct PageRange;
struct PrinterCapsAndDefaults;
}  // namespace printing

// Acts as the service-side host to a utility child process. A
// utility process is a short-lived sandboxed process that is created to run
// a specific task.
class ServiceUtilityProcessHost : public ServiceChildProcessHost {
 public:
  // Consumers of ServiceUtilityProcessHost must implement this interface to
  // get results back.  All functions are called on the thread passed along
  // to ServiceUtilityProcessHost.
  class Client : public base::RefCountedThreadSafe<Client> {
   public:
    Client() {}

    // Called when the child process died before a reply was receieved.
    virtual void OnChildDied() {}

    // Called when at least one page in the specified PDF has been rendered
    // successfully into |metafile|.
    virtual void OnRenderPDFPagesToMetafileSucceeded(
        const printing::Emf& metafile,
        int highest_rendered_page_number) {}
    // Called when no page in the passed in PDF could be rendered.
    virtual void OnRenderPDFPagesToMetafileFailed() {}

    // Called when the printer capabilities and defaults have been
    // retrieved successfully.
    virtual void OnGetPrinterCapsAndDefaultsSucceeded(
        const std::string& printer_name,
        const printing::PrinterCapsAndDefaults& caps_and_defaults) {}

    // Called when the printer capabilities and defaults could not be
    // retrieved successfully.
    virtual void OnGetPrinterCapsAndDefaultsFailed(
        const std::string& printer_name) {}

   protected:
    virtual ~Client() {}

   private:
    friend class base::RefCountedThreadSafe<Client>;
    friend class ServiceUtilityProcessHost;

    // Invoked when a metafile file is ready.
    void MetafileAvailable(const FilePath& metafile_path,
                           int highest_rendered_page_number);

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  ServiceUtilityProcessHost(Client* client,
                            base::MessageLoopProxy* client_message_loop_proxy);
  virtual ~ServiceUtilityProcessHost();

  // Starts a process to render the specified pages in the given PDF file into
  // a metafile. Currently only implemented for Windows. If the PDF has fewer
  // pages than the specified page ranges, it will render as many as available.
  bool StartRenderPDFPagesToMetafile(
      const FilePath& pdf_path,
      const printing::PdfRenderSettings& render_settings,
      const std::vector<printing::PageRange>& page_ranges);

  // Starts a process to get capabilities and defaults for the specified
  // printer. Used on Windows to isolate the service process from printer driver
  // crashes by executing this in a separate process. The process does not run
  // in a sandbox.
  bool StartGetPrinterCapsAndDefaults(const std::string& printer_name);

 protected:
  // Allows this method to be overridden for tests.
  virtual FilePath GetUtilityProcessCmd();

  // ChildProcessHost implementation.
  virtual bool CanShutdown() OVERRIDE;
  virtual void OnChildDied() OVERRIDE;

 private:
  // Starts a process.  Returns true iff it succeeded. |exposed_dir| is the
  // path to the exposed to the sandbox. This is ignored if |no_sandbox| is
  // true.
  bool StartProcess(bool no_sandbox, const FilePath& exposed_dir);

  // IPC messages:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  // Called when at least one page in the specified PDF has been rendered
  // successfully into metafile_path_;
  void OnRenderPDFPagesToMetafileSucceeded(int highest_rendered_page_number);
  // Called when PDF rendering failed.
  void OnRenderPDFPagesToMetafileFailed();
  // Called when the printer capabilities and defaults have been
  // retrieved successfully.
  void OnGetPrinterCapsAndDefaultsSucceeded(
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);
  // Called when the printer capabilities and defaults could not be
  // retrieved successfully.
  void OnGetPrinterCapsAndDefaultsFailed(const std::string& printer_name);

  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<Client> client_;
  scoped_refptr<base::MessageLoopProxy> client_message_loop_proxy_;
  bool waiting_for_reply_;
  // The path to the temp file where the metafile will be written to.
  FilePath metafile_path_;
  // The temporary folder created for the metafile.
  scoped_ptr<ScopedTempDir> scratch_metafile_dir_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUtilityProcessHost);
};

#endif  // CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_
