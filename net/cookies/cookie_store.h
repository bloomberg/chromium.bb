// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#ifndef NET_COOKIES_COOKIE_STORE_H_
#define NET_COOKIES_COOKIE_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"

class GURL;

namespace net {

// An interface for storing and retrieving cookies. Implementations are not
// thread safe, as with most other net classes. All methods must be invoked on
// the network thread, and all callbacks will be calle there.
//
// All async functions may either invoke the callback asynchronously, or they
// may be invoked immediately (prior to return of the asynchronous function).
// Destroying the CookieStore will cancel pending async callbacks.
class NET_EXPORT CookieStore {
 public:
  // The publicly relevant reasons a cookie might be changed.
  enum class ChangeCause {
    // The cookie was inserted.
    INSERTED,
    // The cookie was changed directly by a consumer's action.
    EXPLICIT,
    // The cookie was deleted, but no more details are known.
    UNKNOWN_DELETION,
    // The cookie was automatically removed due to an insert operation that
    // overwrote it.
    OVERWRITE,
    // The cookie was automatically removed as it expired.
    EXPIRED,
    // The cookie was automatically evicted during garbage collection.
    EVICTED,
    // The cookie was overwritten with an already-expired expiration date.
    EXPIRED_OVERWRITE
  };

  // Returns whether |cause| is one that could be a reason for deleting a
  // cookie. This function assumes that ChangeCause::EXPLICIT is a reason for
  // deletion.
  static bool ChangeCauseIsDeletion(ChangeCause cause);

  // Callback definitions.
  typedef base::OnceCallback<void(const CookieList& cookies)>
      GetCookieListCallback;
  typedef base::OnceCallback<void(bool success)> SetCookiesCallback;
  typedef base::OnceCallback<void(uint32_t num_deleted)> DeleteCallback;

  typedef base::Callback<void(const CanonicalCookie& cookie, ChangeCause cause)>
      CookieChangedCallback;
  typedef base::CallbackList<void(const CanonicalCookie& cookie,
                                  ChangeCause cause)>
      CookieChangedCallbackList;
  typedef base::Callback<bool(const CanonicalCookie& cookie)> CookiePredicate;

  class CookieChangedSubscription {
   public:
    virtual ~CookieChangedSubscription(){};
  };

  virtual ~CookieStore();

  // Sets the cookies specified by |cookie_list| returned from |url|
  // with options |options| in effect.  Expects a cookie line, like
  // "a=1; domain=b.com".
  //
  // Fails either if the cookie is invalid or if this is a non-HTTPONLY cookie
  // and it would overwrite an existing HTTPONLY cookie.
  // Returns true if the cookie is successfully set.
  virtual void SetCookieWithOptionsAsync(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options,
                                         SetCookiesCallback callback) = 0;

  // Set the cookie on the cookie store.  |cookie.IsCanonical()| must
  // be true.  |secure_source| indicates if the source of the setting
  // may be considered secure (if from a URL, the scheme is
  // cryptographic), and |modify_http_only| indicates if the source of
  // the setting may modify http_only cookies.  The current time will
  // be used in place of a null creation time.
  virtual void SetCanonicalCookieAsync(std::unique_ptr<CanonicalCookie> cookie,
                                       bool secure_source,
                                       bool modify_http_only,
                                       SetCookiesCallback callback) = 0;

  // Obtains a CookieList for the given |url| and |options|. The returned
  // cookies are passed into |callback|, ordered by longest path, then earliest
  // creation date.
  virtual void GetCookieListWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      GetCookieListCallback callback) = 0;

  // Returns all cookies associated with |url|, including http-only, and
  // same-site cookies. The returned cookies are ordered by longest path, then
  // by earliest creation date, and are not marked as having been accessed.
  //
  // TODO(mkwst): This method is deprecated, and should be removed, either by
  // updating callsites to use 'GetCookieListWithOptionsAsync' with an explicit
  // CookieOptions, or by changing CookieOptions' defaults.
  void GetAllCookiesForURLAsync(const GURL& url,
                                GetCookieListCallback callback);

  // Returns all the cookies, for use in management UI, etc. This does not mark
  // the cookies as having been accessed. The returned cookies are ordered by
  // longest path, then by earliest creation date.
  virtual void GetAllCookiesAsync(GetCookieListCallback callback) = 0;

  // Deletes all cookies that might apply to |url| that have |cookie_name|.
  virtual void DeleteCookieAsync(const GURL& url,
                                 const std::string& cookie_name,
                                 base::OnceClosure callback) = 0;

  // Deletes one specific cookie. |cookie| must have been returned by a previous
  // query on this CookieStore. Invokes |callback| with 1 if a cookie was
  // deleted, 0 otherwise.
  virtual void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                          DeleteCallback callback) = 0;

  // Deletes all of the cookies that have a creation_date greater than or equal
  // to |delete_begin| and less than |delete_end|
  // Calls |callback| with the number of cookies deleted.
  virtual void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            DeleteCallback callback) = 0;

  // Deletes all of the cookies that match the given predicate and that have a
  // creation_date greater than or equal to |delete_begin| and smaller than
  // |delete_end|. Null times do not cap their ranges (i.e.
  // |delete_end.is_null()| would mean that there is no time after which
  // cookies are not deleted).  This includes all http_only and secure
  // cookies. Avoid deleting cookies that could leave websites with a
  // partial set of visible cookies.
  // Calls |callback| with the number of cookies deleted.
  virtual void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const CookiePredicate& predicate,
      DeleteCallback callback) = 0;

  virtual void DeleteSessionCookiesAsync(DeleteCallback) = 0;

  // Deletes all cookies in the store.
  void DeleteAllAsync(DeleteCallback callback);

  // Flush the backing store (if any) to disk and post the given callback when
  // done.
  virtual void FlushStore(base::OnceClosure callback) = 0;

  // Protects session cookies from deletion on shutdown, if the underlying
  // CookieStore implemention is currently configured to store them to disk.
  // Otherwise, does nothing.
  virtual void SetForceKeepSessionState();

  // Add a callback to be notified when the set of cookies named |name| that
  // would be sent for a request to |url| changes. The returned handle is
  // guaranteed not to hold a hard reference to the CookieStore object.
  //
  // |callback| will be called when a cookie is added or removed. |callback| is
  // passed the respective |cookie| which was added to or removed from the
  // cookies and a boolean indicating if the cookies was removed or not.
  // |callback| is guaranteed not to be called after the return handled is
  // destroyed.
  //
  // Note that |callback| is called twice when a cookie is updated: once for
  // the removal of the existing cookie and once for the adding the new cookie.
  //
  // Note that this method consumes memory and CPU per (url, name) pair ever
  // registered that are still consumed even after all subscriptions for that
  // (url, name) pair are removed. If this method ever needs to support an
  // unbounded amount of such pairs, this contract needs to change and
  // implementors need to be improved to not behave this way.
  //
  // The callback must not synchronously modify another cookie.
  virtual std::unique_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) = 0;

  // Add a callback to be notified on all cookie changes (with a few
  // bookkeeping exceptions; see kChangeCauseMapping in
  // cookie_monster.cc).  See the comment on AddCallbackForCookie for details
  // on callback behavior.
  virtual std::unique_ptr<CookieChangedSubscription> AddCallbackForAllChanges(
      const CookieChangedCallback& callback) = 0;

  // Returns true if this cookie store is ephemeral, and false if it is backed
  // by some sort of persistence layer.
  // TODO(nharper): Remove this method once crbug.com/548423 has been closed.
  virtual bool IsEphemeral() = 0;
  void SetChannelIDServiceID(int id);
  int GetChannelIDServiceID();

 protected:
  CookieStore();
  int channel_id_service_id_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_H_
