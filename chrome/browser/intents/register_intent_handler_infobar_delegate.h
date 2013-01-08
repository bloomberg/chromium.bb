// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INTENTS_REGISTER_INTENT_HANDLER_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "webkit/glue/web_intent_service_data.h"

#if defined(UNIT_TEST)
#include "base/memory/scoped_ptr.h"
#endif

class WebIntentsRegistry;
class FaviconService;
class GURL;
namespace content {
class WebContents;
}

// The InfoBar used to request permission for a site to be registered as an
// Intent handler.
class RegisterIntentHandlerInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Checks whether the intent service specified by |data| exists.  If not, and
  // |web_contents| is in a non-incognito, web-intents-enabled profile, creates
  // a register intent handler delegate and adds it to the InfoBarService for
  // |web_contents|.
  static void Create(content::WebContents* web_contents,
                     const webkit_glue::WebIntentServiceData& data);

#if defined(UNIT_TEST)
  static scoped_ptr<RegisterIntentHandlerInfoBarDelegate> Create(
      WebIntentsRegistry* registry,
      const webkit_glue::WebIntentServiceData& data) {
    return scoped_ptr<RegisterIntentHandlerInfoBarDelegate>(
        new RegisterIntentHandlerInfoBarDelegate(NULL, registry, data, NULL,
                                                 GURL()));
  }
#endif

  // ConfirmInfoBarDelegate implementation.
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

 private:
  RegisterIntentHandlerInfoBarDelegate(
      InfoBarService* infobar_service,
      WebIntentsRegistry* registry,
      const webkit_glue::WebIntentServiceData& service,
      FaviconService* favicon_service,
      const GURL& origin_url);

  // Finishes the work of Create().  This is called back from the
  // WebIntentsRegistry once it determines whether the requested intent service
  // exists.
  static void CreateContinuation(
      InfoBarService* infobar_service,
      WebIntentsRegistry* registry,
      const webkit_glue::WebIntentServiceData& service,
      FaviconService* favicon_service,
      const GURL& origin_url,
      bool provider_exists);

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
