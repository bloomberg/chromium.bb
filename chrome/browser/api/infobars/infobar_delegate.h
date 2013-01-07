// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_INFOBARS_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_API_INFOBARS_INFOBAR_DELEGATE_H_

#include "base/basictypes.h"
#include "base/string16.h"
#include "webkit/glue/window_open_disposition.h"

class AlternateNavInfoBarDelegate;
class AutoLoginInfoBarDelegate;
class ConfirmInfoBarDelegate;
class ExtensionInfoBarDelegate;
class InfoBar;
class InfoBarService;
class InsecureContentInfoBarDelegate;
class MediaStreamInfoBarDelegate;
class PluginInstallerInfoBarDelegate;
class RegisterProtocolHandlerInfoBarDelegate;
class SavePasswordInfoBarDelegate;
class ThemeInstalledInfoBarDelegate;
class ThreeDAPIInfoBarDelegate;
class TranslateInfoBarDelegate;

namespace gfx {
class Image;
}
namespace content {
struct LoadCommittedDetails;
}

// An interface implemented by objects wishing to control an InfoBar.
// Implementing this interface is not sufficient to use an InfoBar, since it
// does not map to a specific InfoBar type. Instead, you must implement
// ConfirmInfoBarDelegate, or override with your own delegate for your own
// InfoBar variety.
class InfoBarDelegate {
 public:
  // The type of the infobar. It controls its appearance, such as its background
  // color.
  enum Type {
    WARNING_TYPE,
    PAGE_ACTION_TYPE,
  };

  enum InfoBarAutomationType {
    CONFIRM_INFOBAR,
    ONE_CLICK_LOGIN_INFOBAR,
    PASSWORD_INFOBAR,
    RPH_INFOBAR,
    UNKNOWN_INFOBAR,
  };

  virtual ~InfoBarDelegate();

  virtual InfoBarAutomationType GetInfoBarAutomationType() const;

  // Called to create the InfoBar. Implementation of this method is
  // platform-specific.
  virtual InfoBar* CreateInfoBar(InfoBarService* owner) = 0;

  // TODO(pkasting): Move to InfoBar once InfoBars own their delegates.
  InfoBarService* owner() { return owner_; }

  void clear_owner() { owner_ = NULL; }

  // Returns true if the supplied |delegate| is equal to this one. Equality is
  // left to the implementation to define. This function is called by the
  // InfoBarService when determining whether or not a delegate should be
  // added because a matching one already exists. If this function returns true,
  // the InfoBarService will not add the new delegate because it considers
  // one to already be present.
  virtual bool EqualsDelegate(InfoBarDelegate* delegate) const;

  // Returns true if the InfoBar should be closed automatically after the page
  // is navigated. By default this returns true if the navigation is to a new
  // page (not including reloads).  Subclasses wishing to change this behavior
  // can override either this function or ShouldExpireInternal(), depending on
  // what level of control they need.
  virtual bool ShouldExpire(const content::LoadCommittedDetails& details) const;

  // Called when the user clicks on the close button to dismiss the infobar.
  virtual void InfoBarDismissed();

  // Called after the InfoBar is closed. Deletes |this|.
  // TODO(pkasting): Get rid of this and delete delegates directly.
  void InfoBarClosed();

  // Return the icon to be shown for this InfoBar. If the returned Image is
  // NULL, no icon is shown.
  virtual gfx::Image* GetIcon() const;

  // Returns the type of the infobar.  The type determines the appearance (such
  // as background color) of the infobar.
  virtual Type GetInfoBarType() const;

  // Type-checking downcast routines:
  virtual AlternateNavInfoBarDelegate* AsAlternateNavInfoBarDelegate();
  virtual AutoLoginInfoBarDelegate* AsAutoLoginInfoBarDelegate();
  virtual ConfirmInfoBarDelegate* AsConfirmInfoBarDelegate();
  virtual ExtensionInfoBarDelegate* AsExtensionInfoBarDelegate();
  virtual InsecureContentInfoBarDelegate* AsInsecureContentInfoBarDelegate();
  virtual MediaStreamInfoBarDelegate* AsMediaStreamInfoBarDelegate();
  virtual RegisterProtocolHandlerInfoBarDelegate*
      AsRegisterProtocolHandlerInfoBarDelegate();
  virtual ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate();
  virtual ThreeDAPIInfoBarDelegate* AsThreeDAPIInfoBarDelegate();
  virtual TranslateInfoBarDelegate* AsTranslateInfoBarDelegate();

 protected:
  // If |contents| is non-NULL, its active entry's unique ID will be stored
  // using StoreActiveEntryUniqueID automatically.
  explicit InfoBarDelegate(InfoBarService* infobar_service);

  // Store the unique id for the active entry in our WebContents, to be used
  // later upon navigation to determine if this InfoBarDelegate should be
  // expired.
  void StoreActiveEntryUniqueID();

  int contents_unique_id() const { return contents_unique_id_; }

  // Returns true if the navigation is to a new URL or a reload occured.
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const;

  // Removes ourself from |owner_| if we haven't already been removed.
  // TODO(pkasting): Move to InfoBar.
  void RemoveSelf();

 private:
  // The unique id of the active NavigationEntry of the WebContents that we were
  // opened for. Used to help expire on navigations.
  int contents_unique_id_;

  // TODO(pkasting): Remove.
  InfoBarService* owner_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarDelegate);
};

#endif  // CHROME_BROWSER_API_INFOBARS_INFOBAR_DELEGATE_H_
