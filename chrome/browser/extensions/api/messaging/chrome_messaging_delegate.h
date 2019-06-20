// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_

#include "extensions/browser/api/messaging/messaging_delegate.h"

namespace extensions {

// Helper class for Chrome-specific features of the extension messaging API.
class ChromeMessagingDelegate : public MessagingDelegate {
 public:
  ChromeMessagingDelegate();
  ~ChromeMessagingDelegate() override;

  // MessagingDelegate:
  PolicyPermission IsNativeMessagingHostAllowed(
      content::BrowserContext* browser_context,
      const std::string& native_host_name) override;
  std::unique_ptr<base::DictionaryValue> MaybeGetTabInfo(
      content::WebContents* web_contents) override;
  content::WebContents* GetWebContentsByTabId(
      content::BrowserContext* browser_context,
      int tab_id) override;
  std::unique_ptr<MessagePort> CreateReceiverForTab(
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      const std::string& extension_id,
      const PortId& receiver_port_id,
      content::WebContents* receiver_contents,
      int receiver_frame_id) override;
  std::unique_ptr<MessagePort> CreateReceiverForNativeApp(
      content::BrowserContext* browser_context,
      base::WeakPtr<MessagePort::ChannelDelegate> channel_delegate,
      content::RenderFrameHost* source,
      const std::string& extension_id,
      const PortId& receiver_port_id,
      const std::string& native_app_name,
      bool allow_user_level,
      std::string* error_out) override;
  void QueryIncognitoConnectability(
      content::BrowserContext* context,
      const Extension* extension,
      content::WebContents* web_contents,
      const GURL& url,
      const base::Callback<void(bool)>& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMessagingDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_CHROME_MESSAGING_DELEGATE_H_
