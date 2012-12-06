// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_

#include <string>
#include <vector>

#include "base/observer_list.h"

namespace extensions {

class Extension;
class ExtensionPrefs;

// A blacklist of extensions.
class Blacklist {
 public:
  class Observer {
   public:
    // Observes |blacklist| on construction and unobserves on destruction.
    explicit Observer(Blacklist* blacklist);

    virtual void OnBlacklistUpdated() = 0;

   protected:
    virtual ~Observer();

   private:
    Blacklist* blacklist_;
  };

  // |prefs_| must outlive this.
  explicit Blacklist(ExtensionPrefs* prefs);

  ~Blacklist();

  // Gets whether an extension is blacklisted.
  //
  // Note that this doesn't entirely determine whether an extension is allowed
  // to be loaded; there are other considerations (e.g. admin settings).
  // See extensions::ManagementPolicy (in particular UserMayLoad).
  bool IsBlacklisted(const std::string& extension_id) const;
  bool IsBlacklisted(const Extension* extension) const;

  // Sets the blacklist from the updater to contain the extension IDs in |ids|
  void SetFromUpdater(const std::vector<std::string>& ids,
                      const std::string& version);

  // Adds/removes an observer to the blacklist.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  ObserverList<Observer> observers_;

  ExtensionPrefs* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(Blacklist);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_H_
