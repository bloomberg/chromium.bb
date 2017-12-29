// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_IOS_H_

#include "base/macros.h"
#include "components/toolbar/toolbar_model_delegate.h"

class WebStateList;

namespace web {
class NavigationItem;
class WebState;
}

// Implementation of ToolbarModelDelegate which uses an instance of
// TabModel in order to fulfil its duties.
class ToolbarModelDelegateIOS : public ToolbarModelDelegate {
 public:
  // |web_state_list| must outlive this ToolbarModelDelegateIOS object.
  explicit ToolbarModelDelegateIOS(WebStateList* web_state_list);
  ~ToolbarModelDelegateIOS() override;

  // Returns the active WebState.
  web::WebState* GetActiveWebState() const;

 private:
  // Helper method to extract the NavigationItem from which the states are
  // retrieved. If this returns null (which can happens during initialization),
  // default values are used.
  web::NavigationItem* GetNavigationItem() const;

  // ToolbarModelDelegate implementation:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldDisplayURL() const override;
  SecurityLevel GetSecurityLevel() const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;
  bool FailsMalwareCheck() const override;
  const gfx::VectorIcon* GetVectorIconOverride() const override;
  bool IsOfflinePage() const override;

  WebStateList* web_state_list_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ToolbarModelDelegateIOS);
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_IOS_H_
