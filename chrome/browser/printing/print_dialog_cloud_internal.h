// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_INTERNAL_H_
#define CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_INTERNAL_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class CloudPrintHtmlDialogDelegateTest;

namespace base {
class ListValue;
class StringValue;
class Value;
}

namespace internal_cloud_print_helpers {

// Small class to virtualize a few functions to aid with unit testing.
class CloudPrintDataSenderHelper {
 public:
  explicit CloudPrintDataSenderHelper(WebUI* web_ui) : web_ui_(web_ui) {}
  virtual ~CloudPrintDataSenderHelper() {}

  // Virtualize the overrides of these three functions from WebUI to
  // facilitate unit testing.
  virtual void CallJavascriptFunction(const std::wstring& function_name);
  virtual void CallJavascriptFunction(const std::wstring& function_name,
                                      const base::Value& arg);
  virtual void CallJavascriptFunction(const std::wstring& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2);
  virtual void CallJavascriptFunction(const std::wstring& function_name,
                                      const base::Value& arg1,
                                      const base::Value& arg2,
                                      const base::Value& arg3);

 private:
  WebUI* web_ui_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintDataSenderHelper);
};

// Small helper class to get the print data loaded in from the PDF
// file (on the FILE thread) and send it to the print dialog contents
// (on the IO thread), allowing for cancellation.
class CloudPrintDataSender
    : public base::RefCountedThreadSafe<CloudPrintDataSender> {
 public:
  // The owner of this object is also expected to own and control the
  // lifetime of the helper.
  CloudPrintDataSender(CloudPrintDataSenderHelper* helper,
                       const string16& print_job_title,
                       const string16& print_ticket,
                       const std::string& file_type);

  // Calls to read in the PDF file (on the FILE thread) then send that
  // information to the dialog renderer (on the IO thread).  We know
  // that the WebUI pointer lifetime will outlast us, so we should be
  // good.
  void ReadPrintDataFile(const FilePath& path_to_file);
  void SendPrintDataFile();

  // Cancels any ramining part of the task by clearing out the WebUI
  // helper_ ptr.
  void CancelPrintDataFile();

 private:
  friend class base::RefCountedThreadSafe<CloudPrintDataSender>;
  virtual ~CloudPrintDataSender();

  base::Lock lock_;
  CloudPrintDataSenderHelper* volatile helper_;
  scoped_ptr<base::StringValue> print_data_;
  string16 print_job_title_;
  string16 print_ticket_;
  std::string file_type_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintDataSender);
};

class CloudPrintHtmlDialogDelegate;

// The CloudPrintFlowHandler connects the state machine (the UI delegate)
// to the dialog backing HTML and JS by providing WebUIMessageHandler
// functions for the JS to use.  This include refreshing the page
// setup parameters (which will cause a re-generation of the PDF in
// the renderer process - do we want a progress throbber shown?
// Probably..), and packing up the PDF and job parameters and sending
// them to the cloud.
class CloudPrintFlowHandler : public WebUIMessageHandler,
                              public content::NotificationObserver {
 public:
  CloudPrintFlowHandler(const FilePath& path_to_file,
                        const string16& print_job_title,
                        const string16& print_ticket,
                        const std::string& file_type);
  virtual ~CloudPrintFlowHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callbacks from the page.
  void HandleShowDebugger(const base::ListValue* args);
  void HandleSendPrintData(const base::ListValue* args);
  void HandleSetPageParameters(const base::ListValue* args);

  virtual void SetDialogDelegate(CloudPrintHtmlDialogDelegate *delegate);
  void StoreDialogClientSize() const;

 private:
  virtual scoped_refptr<CloudPrintDataSender> CreateCloudPrintDataSender();

  // Call to get the debugger loaded on our hosted dialog page
  // specifically.  Since we're not in an official browser tab, only
  // way to get the debugger going.
  void ShowDebugger();

  void CancelAnyRunningTask();

  CloudPrintHtmlDialogDelegate* dialog_delegate_;
  content::NotificationRegistrar registrar_;
  FilePath path_to_file_;
  string16 print_job_title_;
  string16 print_ticket_;
  std::string file_type_;
  scoped_refptr<CloudPrintDataSender> print_data_sender_;
  scoped_ptr<CloudPrintDataSenderHelper> print_data_helper_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintFlowHandler);
};

// State machine used to run the printing dialog.  This class is used
// to open and run the html dialog and deletes itself when the dialog
// is closed.
class CloudPrintHtmlDialogDelegate : public HtmlDialogUIDelegate {
 public:
  CloudPrintHtmlDialogDelegate(const FilePath& path_to_file,
                               int width, int height,
                               const std::string& json_arguments,
                               const string16& print_job_title,
                               const string16& print_ticket,
                               const std::string& file_type,
                               bool modal,
                               bool delete_on_close);
  virtual ~CloudPrintHtmlDialogDelegate();

  // HTMLDialogUIDelegate implementation:
  virtual bool IsDialogModal() const OVERRIDE;
  virtual string16 GetDialogTitle() const OVERRIDE;
  virtual GURL GetDialogContentURL() const OVERRIDE;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE;
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE;
  virtual std::string GetDialogArgs() const OVERRIDE;
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE;
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog)
      OVERRIDE;
  virtual bool ShouldShowDialogTitle() const OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;

 private:
  friend class ::CloudPrintHtmlDialogDelegateTest;

  // For unit testing.
  CloudPrintHtmlDialogDelegate(CloudPrintFlowHandler* flow_handler,
                               int width, int height,
                               const std::string& json_arguments,
                               bool modal,
                               bool delete_on_close);
  void Init(int width, int height, const std::string& json_arguments);

  bool delete_on_close_;
  CloudPrintFlowHandler* flow_handler_;
  bool modal_;
  mutable bool owns_flow_handler_;
  FilePath path_to_file_;

  // The parameters needed to display a modal HTML dialog.
  HtmlDialogUI::HtmlDialogParams params_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintHtmlDialogDelegate);
};

void CreateDialogImpl(const FilePath& path_to_file,
                      const string16& print_job_title,
                      const string16& print_ticket,
                      const std::string& file_type,
                      bool modal,
                      bool delete_on_close);

void Delete(const FilePath& path_to_file);

}  // namespace internal_cloud_print_helpers

#endif  // CHROME_BROWSER_PRINTING_PRINT_DIALOG_CLOUD_INTERNAL_H_
