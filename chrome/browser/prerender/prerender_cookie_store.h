// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_COOKIE_STORE_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_COOKIE_STORE_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

namespace prerender {

// A cookie store which keeps track of provisional changes to the cookie monster
// of an underlying request context (called the default cookie monster).
// Initially, it will proxy read requests to the default cookie monster, and
// copy on write keys that are being modified into a private cookie monster.
// Reads for these will then happen from the private cookie monster.
// Should keys be modified in the default cookie store, the corresponding
// prerender should be aborted.
// This class also keeps a log of all cookie transactions. Once ApplyChanges
// is called, the changes will be applied to the default cookie monster,
// and any future requests to this object will simply be forwarded to the
// default cookie monster. After ApplyChanges is called, the prerender tracker,
// which "owns" the PrerenderCookieStore reference, will remove its entry for
// the PrerenderCookieStore. Therefore, after ApplyChanges is called, the
// object will only stick around (and exhibit forwarding mode) as long as
// eg pending requests hold on to its reference.
class PrerenderCookieStore : public net::CookieStore {
 public:
  // Creates a PrerenderCookieStore using the default cookie monster provided
  // by the URLRequestContext. The underlying cookie store must be loaded,
  // ie it's call to loaded() must return true.
  // Otherwise, copying cookie data between the prerender cookie store
  // (used to only commit cookie changes once a prerender is shown) would
  // not work synchronously, which would complicate the code.
  // |cookie_conflict_cb| will be called when a cookie conflict is detected.
  // The callback will be run on the UI thread.
  PrerenderCookieStore(scoped_refptr<net::CookieMonster> default_cookie_store_,
                       const base::Closure& cookie_conflict_cb);

  // CookieStore implementation
  virtual void SetCookieWithOptionsAsync(
      const GURL& url,
      const std::string& cookie_line,
      const net::CookieOptions& options,
      const SetCookiesCallback& callback) OVERRIDE;

  virtual void GetCookiesWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      const GetCookiesCallback& callback) OVERRIDE;

  virtual void GetAllCookiesForURLAsync(
      const GURL& url,
      const GetCookieListCallback& callback) OVERRIDE;

  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 const base::Closure& callback) OVERRIDE;

  // All the following methods should not be used in the scenarios where
  // a PrerenderCookieStore is used. This will be checked via NOTREACHED().
  // Should PrerenderCookieStore used in contexts requiring these, they will
  // need to be implemented first. They are only intended to be called on the
  // IO thread.

  virtual void DeleteAllCreatedBetweenAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const DeleteCallback& callback) OVERRIDE;

  virtual void DeleteAllCreatedBetweenForHostAsync(
      const base::Time delete_begin,
      const base::Time delete_end,
      const GURL& url,
      const DeleteCallback& callback) OVERRIDE;

  virtual void DeleteSessionCookiesAsync(const DeleteCallback&) OVERRIDE;

  virtual net::CookieMonster* GetCookieMonster() OVERRIDE;

  // Commits the changes made to the underlying cookie store, and switches
  // into forwarding mode. To be called on the IO thread.
  // |cookie_change_urls| will be populated with all URLs for which cookies
  // were updated.
  void ApplyChanges(std::vector<GURL>* cookie_change_urls);

  // Called when a cookie for a URL is changed in the underlying default cookie
  // store. To be called on the IO thread. If the key corresponding to the URL
  // was copied or read, the prerender will be cancelled.
  void OnCookieChangedForURL(net::CookieMonster* cookie_monster,
                             const GURL& url);

  net::CookieMonster* default_cookie_monster() {
    return default_cookie_monster_.get();
  }

 private:
  enum CookieOperationType {
    COOKIE_OP_SET_COOKIE_WITH_OPTIONS_ASYNC,
    COOKIE_OP_GET_COOKIES_WITH_OPTIONS_ASYNC,
    COOKIE_OP_GET_ALL_COOKIES_FOR_URL_ASYNC,
    COOKIE_OP_DELETE_COOKIE_ASYNC,
    COOKIE_OP_MAX
  };

  struct CookieOperation {
    CookieOperationType op;
    GURL url;
    net::CookieOptions options;
    std::string cookie_line;
    std::string cookie_name;
    CookieOperation();
    ~CookieOperation();
  };

  virtual ~PrerenderCookieStore();

  // Gets the appropriate cookie store for the operation provided, and pushes
  // it back on the log of cookie operations performed.
  net::CookieStore* GetCookieStoreForCookieOpAndLog(const CookieOperation& op);

  // Indicates whether the changes have already been applied (ie the prerender
  // has been shown), and we are merely in forwarding mode;
  bool in_forwarding_mode_;

  // The default cookie monster.
  scoped_refptr<net::CookieMonster> default_cookie_monster_;

  // A cookie monster storing changes made by the prerender.
  // Entire keys are copied from default_cookie_monster_ on change, and then
  // modified.
  scoped_refptr<net::CookieMonster> changes_cookie_monster_;

  // Log of cookie operations performed
  std::vector<CookieOperation> cookie_ops_;

  // The keys which have been copied on write to |changes_cookie_monster_|.
  std::set<std::string> copied_keys_;

  // Keys which have been read (but not necessarily been modified).
  std::set<std::string> read_keys_;

  // Callback when a cookie conflict was detected
  base::Closure cookie_conflict_cb_;

  // Indicates whether a cookie conflict has been detected yet.
  bool cookie_conflict_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderCookieStore);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_COOKIE_STORE_H_
