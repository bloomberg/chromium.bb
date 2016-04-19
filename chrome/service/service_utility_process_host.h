// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_
#define CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/child_process_host_delegate.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class CommandLine;
class File;
class FilePath;
class ScopedTempDir;
}  // namespace base

namespace content {
class ChildProcessHost;
}

namespace printing {
class MetafilePlayer;
class PdfRenderSettings;
struct PageRange;
struct PrinterCapsAndDefaults;
struct PrinterSemanticCapsAndDefaults;
}  // namespace printing

// Acts as the service-side host to a utility child process. A
// utility process is a short-lived sandboxed process that is created to run
// a specific task.
class ServiceUtilityProcessHost : public content::ChildProcessHostDelegate {
 public:
  // Consumers of ServiceUtilityProcessHost must implement this interface to
  // get results back.  All functions are called on the thread passed along
  // to ServiceUtilityProcessHost.
  class Client : public base::RefCountedThreadSafe<Client> {
   public:
    Client() {}

    // Called when the child process died before a reply was receieved.
    virtual void OnChildDied() {}

    virtual void OnRenderPDFPagesToMetafilePageDone(
        float scale_factor,
        const printing::MetafilePlayer& emf) {}

    // Called when at all pages in the PDF has been rendered.
    virtual void OnRenderPDFPagesToMetafileDone(bool success) {}

    // Called when the printer capabilities and defaults have been
    // retrieved successfully or if retrieval failed.
    virtual void OnGetPrinterCapsAndDefaults(
        bool succedded,
        const std::string& printer_name,
        const printing::PrinterCapsAndDefaults& caps_and_defaults) {}

    // Called when the printer capabilities and defaults have been
    // retrieved successfully or if retrieval failed.
    virtual void OnGetPrinterSemanticCapsAndDefaults(
        bool succedded,
        const std::string& printer_name,
        const printing::PrinterSemanticCapsAndDefaults& caps_and_defaults) {}

   protected:
    virtual ~Client() {}

   private:
    friend class base::RefCountedThreadSafe<Client>;
    friend class ServiceUtilityProcessHost;

    // Invoked when a metafile file is ready.
    // Returns true if metafile successfully loaded from |file|.
    bool MetafileAvailable(float scale_factor, base::File file);

    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  ServiceUtilityProcessHost(Client* client,
                            base::SingleThreadTaskRunner* client_task_runner);
  ~ServiceUtilityProcessHost() override;

  // Starts a process to render the specified pages in the given PDF file into
  // a metafile. Currently only implemented for Windows. If the PDF has fewer
  // pages than the specified page ranges, it will render as many as available.
  bool StartRenderPDFPagesToMetafile(
      const base::FilePath& pdf_path,
      const printing::PdfRenderSettings& render_settings);

  // Starts a process to get capabilities and defaults for the specified
  // printer. Used on Windows to isolate the service process from printer driver
  // crashes by executing this in a separate process. The process does not run
  // in a sandbox.
  bool StartGetPrinterCapsAndDefaults(const std::string& printer_name);

  // Starts a process to get capabilities and defaults for the specified
  // printer. Used on Windows to isolate the service process from printer driver
  // crashes by executing this in a separate process. The process does not run
  // in a sandbox. Returns result as printing::PrinterSemanticCapsAndDefaults.
  bool StartGetPrinterSemanticCapsAndDefaults(const std::string& printer_name);

 protected:
  bool Send(IPC::Message* msg);

  // Allows this method to be overridden for tests.
  virtual base::FilePath GetUtilityProcessCmd();

  // ChildProcessHostDelegate implementation:
  void OnChildDisconnected() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  const base::Process& GetProcess() const override;

 private:
  // Starts a process.  Returns true iff it succeeded.
  bool StartProcess(bool no_sandbox);

  // Launch the child process synchronously.
  bool Launch(base::CommandLine* cmd_line, bool no_sandbox);

  base::ProcessHandle handle() const { return process_.Handle(); }

  void OnMetafileSpooled(bool success);
  void OnPDFToEmfFinished(bool success);

  // Messages handlers:
  void OnRenderPDFPagesToMetafilesPageCount(int page_count);
  void OnRenderPDFPagesToMetafilesPageDone(bool success, float scale_factor);
  void OnGetPrinterCapsAndDefaultsSucceeded(
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);
  void OnGetPrinterCapsAndDefaultsFailed(const std::string& printer_name);
  void OnGetPrinterSemanticCapsAndDefaultsSucceeded(
      const std::string& printer_name,
      const printing::PrinterSemanticCapsAndDefaults& caps_and_defaults);
  void OnGetPrinterSemanticCapsAndDefaultsFailed(
      const std::string& printer_name);

  std::unique_ptr<content::ChildProcessHost> child_process_host_;
  base::Process process_;
  // A pointer to our client interface, who will be informed of progress.
  scoped_refptr<Client> client_;
  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;
  bool waiting_for_reply_;

  // Start time of operation.
  base::Time start_time_;

  class PdfToEmfState;
  std::unique_ptr<PdfToEmfState> pdf_to_emf_state_;

  base::WeakPtrFactory<ServiceUtilityProcessHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUtilityProcessHost);
};

#endif  // CHROME_SERVICE_SERVICE_UTILITY_PROCESS_HOST_H_
