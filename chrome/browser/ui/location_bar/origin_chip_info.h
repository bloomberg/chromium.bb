// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_ORIGIN_CHIP_INFO_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_ORIGIN_CHIP_INFO_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "extensions/browser/extension_icon_image.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

// This class manages the information to be displayed by the origin chip.
class OriginChipInfo {
 public:
  // The |owner| owns this OriginChipInfo and must outlive it. If the current
  // origin is an extension, the |owner|'s OnExtensionIconImageChanged()
  // method will be called whenever the icon changes.
  OriginChipInfo(extensions::IconImage::Observer* owner, Profile* profile);
  ~OriginChipInfo();

  // Updates the current state. Returns whether the displayable information has
  // changed.
  bool Update(const content::WebContents* web_contents,
              const ToolbarModel* toolbar_model);

  // Returns the label to be displayed by the origin chip.
  const base::string16& label() const { return label_; }

  // Returns the tooltip to be used for the origin chip.
  base::string16 Tooltip() const;

  // Returns the ID of the icon to be displayed by the origin chip. Note that
  // if |owner_|'s OnExtensionIconImageChanged() method was called with
  // anything else than NULL, that icon has precedence over whatever this method
  // returns.
  int icon() const { return icon_; }

  // Returns the full URL.
  const GURL& displayed_url() const { return displayed_url_; }

  // Returns the security level of the current URL.
  ToolbarModel::SecurityLevel security_level() const { return security_level_; }

  // Returns whether the current URL is known to be malware or not.
  bool is_url_malware() const { return is_url_malware_; }

 private:
  // The observer which will receive OnExtensionIconImageChanged() calls.
  extensions::IconImage::Observer* owner_;

  Profile* profile_;

  // State used to determine whether the |label_| and |icon_| need to be
  // recomputed.
  GURL displayed_url_;
  ToolbarModel::SecurityLevel security_level_;
  bool is_url_malware_;

  // The label and icon to be displayed by the origin chip.
  base::string16 label_;
  int icon_;

  // extensions::IconImage instance, used for observing changes in an
  // extension's icon, when the current page belongs to an extension.
  scoped_ptr<extensions::IconImage> extension_icon_image_;

  DISALLOW_COPY_AND_ASSIGN(OriginChipInfo);
};

// This class is left here to prevent breaking the views code. It will be
// removed soon, and the functions will become local to origin_chip.cc.
class OriginChip {
 public:
  // Detects client-side or SB malware/phishing hits. Used to decide whether the
  // origin chip should indicate that the current page has malware or is a known
  // phishing page.
  static bool IsMalware(const GURL& url, const content::WebContents* tab);

  // Gets the label to display on the Origin Chip for the specified URL. The
  // |profile| is needed in case the URL is from an extension.
  static base::string16 LabelFromURLForProfile(const GURL& provided_url,
                                               Profile* profile);
};

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_ORIGIN_CHIP_INFO_H_
