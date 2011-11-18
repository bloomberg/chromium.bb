// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "webkit/glue/web_intent_service_data.h"

class InfoBarTabHelper;
class WebIntentsRegistry;
class FaviconService;
class GURL;

// The InfoBar used to request permission for a site to be registered as an
// Intent handler.
class RegisterIntentHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  RegisterIntentHandlerInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      WebIntentsRegistry* registry,
      const webkit_glue::WebIntentServiceData& service,
      FaviconService* favicon_service,
      const GURL& origin_url);

  // ConfirmInfoBarDelegate implementation.
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // Shows the intent registration infobar if |service| has not already been
  // registered.
  // |infobar_helper| is the infobar controller for the tab in which the infobar
  // may be shown. Must not be NULL.
  // |registry| is the data source for web intents. Must not be NULL.
  // |service| is the candidate service to show the infobar for.
  // |favicon_service| is the favicon service to use. Must not be NULL.
  // |origin_url| is the URL that the intent is registered from.
  static void MaybeShowIntentInfoBar(
      InfoBarTabHelper* infobar_helper,
      WebIntentsRegistry* registry,
      const webkit_glue::WebIntentServiceData& service,
      FaviconService* favicon_service,
      const GURL& origin_url);

 private:
  // The web intents registry to use. Weak pointer.
  WebIntentsRegistry* registry_;

  // The cached intent service data bundle passed up from the renderer.
  webkit_glue::WebIntentServiceData service_;

  // The favicon service to use. Weak pointer.
  FaviconService* favicon_service_;

  // The URL of the page the service was originally registered from.
  GURL origin_url_;

  DISALLOW_COPY_AND_ASSIGN(RegisterIntentHandlerInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
