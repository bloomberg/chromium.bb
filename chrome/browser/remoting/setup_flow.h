// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_FLOW_H_
#define CHROME_BROWSER_REMOTING_SETUP_FLOW_H_

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/remoting/chromoting_host_info.h"
#include "content/browser/webui/web_ui.h"

class ListValue;
class ServiceProcessControl;

namespace remoting {

class SetupFlow;

// SetupFlowStep represents a single step for SetupFlow, e.g. login or
// host registration. When a step is finished, GetNextStep() is called
// to get the step that must follow.
class SetupFlowStep {
 public:
  typedef Callback0::Type DoneCallback;

  SetupFlowStep();
  virtual ~SetupFlowStep();

  // Start the step. Ownership of |done_callback| is given to the
  // function. |done_callback| is called when the step is finished,
  // The callback must be called on the same thread as Start().
  virtual void Start(SetupFlow* flow, DoneCallback* done_callback) = 0;

  // Called to handle |message| received from UI. |args| may be set to
  // NULL.
  virtual void HandleMessage(const std::string& message, const Value* arg) = 0;

  // Called if user closes the dialog.
  virtual void Cancel() = 0;

  // Returns SetupFlowStep object that corresponds to the next
  // step. Must never return NULL.
  virtual SetupFlowStep* GetNextStep() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowStep);
};

// SetupFlowStepBase implements base functions common for all
// SetupFlowStep implementations.
class SetupFlowStepBase : public SetupFlowStep {
 public:
  SetupFlowStepBase();
  ~SetupFlowStepBase();

  // SetupFlowStep implementation.
  virtual void Start(SetupFlow* flow, DoneCallback* done_callback);
  virtual SetupFlowStep* GetNextStep();

 protected:
  SetupFlow* flow() { return flow_; }

  void ExecuteJavascriptInIFrame(const std::wstring& iframe_xpath,
                                 const std::wstring& js);

  // Finish current step. Calls |done_callback| specified in Start().
  // GetNextStep() will return the specified |next_step|.
  void FinishStep(SetupFlowStep* next_step);

  // Called from Start(). Child classes must override this method
  // instead of Start().
  virtual void DoStart() = 0;

 private:
  SetupFlow* flow_;
  scoped_ptr<DoneCallback> done_callback_;
  bool done_;

  // Next step stored between Done() and GetNextStep();
  SetupFlowStep* next_step_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowStepBase);
};

// Base class for error steps. It shows the error message returned by
// GetErrorMessage() and Retry button.
class SetupFlowErrorStepBase : public SetupFlowStepBase {
 public:
  SetupFlowErrorStepBase();
  virtual ~SetupFlowErrorStepBase();

  // SetupFlowStep implementation.
  virtual void HandleMessage(const std::string& message, const Value* arg);
  virtual void Cancel();

 protected:
  virtual void DoStart();

  // Returns error message that is shown to the user.
  virtual string16 GetErrorMessage() = 0;

  // Called when user clicks Retry button. Normally this methoud just
  // calls FinishStep() with an appropriate next step.
  virtual void Retry() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowErrorStepBase);
};

// The last step in the setup flow. This step never finishes, user is
// expected to close dialog after that.
class SetupFlowDoneStep : public SetupFlowStepBase {
 public:
  SetupFlowDoneStep();
  explicit SetupFlowDoneStep(const string16& message);
  virtual ~SetupFlowDoneStep();

  // SetupFlowStep implementation.
  virtual void HandleMessage(const std::string& message, const Value* arg);
  virtual void Cancel();

 protected:
  virtual void DoStart();

 private:
  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowDoneStep);
};

// SetupFlowContext stores data that needs to be passed between
// different setup flow steps.
struct SetupFlowContext {
  SetupFlowContext();
  ~SetupFlowContext();

  std::string login;
  std::string remoting_token;
  std::string talk_token;

  ChromotingHostInfo host_info;
};

// This class is responsible for showing a remoting setup dialog and
// perform operations to fill the content of the dialog and handle
// user actions in the dialog.
//
// Each page in the setup flow may send message to the current
// step. In order to do that it must use send a RemotingSetup message
// and specify message name as the first value in the argument
// list. For example the following code sends Retry message to the
// current step:
//
//     chrome.send("RemotingSetup", ["Retry"])
//
// Assitional message parameters may be provided via send value in the
// arguments list, e.g.:
//
//     chrome.send("RemotingSetup", ["SubmitAuth", auth_data])
//
// In this case auth_data would be passed in
// SetupFlowStep::HandleMessage().
class SetupFlow : public WebUIMessageHandler,
                  public HtmlDialogUIDelegate {
 public:
  virtual ~SetupFlow();

  static SetupFlow* OpenSetupDialog(Profile* profile);

  WebUI* web_ui() { return web_ui_; }
  Profile* profile() { return profile_; }
  SetupFlowContext* context() { return &context_; }

 private:
  explicit SetupFlow(const std::string& args, Profile* profile,
                     SetupFlowStep* first_step);

  // HtmlDialogUIDelegate implementation.
  virtual GURL GetDialogContentURL() const;
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog);
  virtual std::wstring GetDialogTitle() const;
  virtual bool IsDialogModal() const;
  virtual bool ShouldShowDialogTitle() const;

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui);
  virtual void RegisterMessages();

  // Message handlers for the messages we receive from UI.
  void HandleSubmitAuth(const ListValue* args);
  void HandleUIMessage(const ListValue* args);

  void StartCurrentStep();
  void OnStepDone();

  // Pointer to the Web UI. This is provided by RemotingSetupMessageHandler
  // when attached.
  WebUI* web_ui_;

  // The args to pass to the initial page.
  std::string dialog_start_args_;
  Profile* profile_;

  SetupFlowContext context_;

  scoped_ptr<SetupFlowStep> current_step_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlow);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_SETUP_FLOW_H_
