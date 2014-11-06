// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/javascript_dialog_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/app_modal_dialogs/javascript_app_modal_dialog.h"

class JavaScriptDialogExtensionsClient;
class JavaScriptNativeDialogFactory;

class JavaScriptDialogManagerImpl : public content::JavaScriptDialogManager {
 public:
  static JavaScriptDialogManagerImpl* GetInstance();

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           const GURL& origin_url,
                           const std::string& accept_lang,
                           content::JavaScriptMessageType message_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           const DialogClosedCallback& callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             const base::string16& message_text,
                             bool is_reload,
                             const DialogClosedCallback& callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) override;
  void WebContentsDestroyed(content::WebContents* web_contents) override;

  JavaScriptNativeDialogFactory* native_dialog_factory() {
    return native_dialog_factory_.get();
  }

  void SetNativeDialogFactory(
      scoped_ptr<JavaScriptNativeDialogFactory> factory);

  void SetExtensionsClient(
      scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client);

 private:
  friend struct DefaultSingletonTraits<JavaScriptDialogManagerImpl>;

  JavaScriptDialogManagerImpl();
  ~JavaScriptDialogManagerImpl() override;

  base::string16 GetTitle(content::WebContents* web_contents,
                          const GURL& origin_url,
                          const std::string& accept_lang,
                          bool is_alert);

  // Wrapper around a DialogClosedCallback so that we can intercept it before
  // passing it onto the original callback.
  void OnDialogClosed(content::WebContents* web_contents,
                      DialogClosedCallback callback,
                      bool success,
                      const base::string16& user_input);

  // Mapping between the WebContents and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  JavaScriptAppModalDialog::ExtraDataMap javascript_dialog_extra_data_;

  scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client_;
  scoped_ptr<JavaScriptNativeDialogFactory> native_dialog_factory_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogManagerImpl);
};

