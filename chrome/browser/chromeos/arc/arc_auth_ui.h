// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_UI_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_UI_H_

#include "base/macros.h"
#include "components/arc/auth/arc_auth_fetcher.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

namespace arc {

// Shows ARC LSO web view that requires user input.
class ArcAuthUI : public ui::WebDialogDelegate,
                  public ArcAuthFetcher::Delegate {
 public:
  class Delegate {
   public:
    // Called when dialog is closed.
    virtual void OnAuthUIClosed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  ArcAuthUI(content::BrowserContext* browser_context, Delegate* delegate);
  ~ArcAuthUI() override;

  void Close();

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(const content::ContextMenuParams& params) override;
  void OnLoadingStateChanged(content::WebContents* source) override;

  // ArcAuthFetcher::Delegate:
  void OnAuthCodeFetched(const std::string& code) override;
  void OnAuthCodeNeedUI() override;
  void OnAuthCodeFailed() override;

 private:
  // Unowned pointers.
  content::BrowserContext* const browser_context_;
  Delegate* const delegate_;
  const GURL target_url_;

  // Owned by view hierarchy.
  views::Widget* widget_;

  scoped_ptr<ArcAuthFetcher> auth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthUI);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_UI_H_
