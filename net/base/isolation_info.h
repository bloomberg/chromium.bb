// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ISOLATION_INFO_H_
#define NET_BASE_ISOLATION_INFO_H_

#include "base/optional.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

namespace net {

// Class to store information about network stack requests based on the context
// in which they are made. It provides NetworkIsolationKeys, used to shard
// storage, and SiteForCookies, used determine when to send same site cookies.
// The IsolationInfo is typically the same for all subresource requests made in
// the context of the same frame, but may be different for different frames
// within a page. The IsolationInfo associated with requests for frames may
// change as redirects are followed, and this class also contains the logic on
// how to do that.
//
// The SiteForCookies logic in this class is currently unused, but will
// eventually replace the logic in URLRequest/RedirectInfo for tracking and
// updating that value.
class NET_EXPORT IsolationInfo {
 public:
  // The update-on-redirect patterns.
  //
  // In general, almost everything should use kUpdateNothing, as a
  // kUpdateTopFrame request accidentally sent or redirected to an attacker
  // allows cross-site tracking, and kUpdateFrameOnly allows information
  // leaks between sites that iframe each other. Anything that uses
  // kUpdateTopFrame should be user triggered and user visible, like a main
  // frame navigation or downloads.
  //
  // The RedirectMode is a core part of an IsolationInfo, and using an
  // IsolationInfo with one value to create an IsolationInfo with another
  // RedirectMode is generally not a good idea, unless the RedirectMode of the
  // new IsolationInfo is kUpdateNothing.
  enum class RedirectMode {
    // Update top level origin, frame origin, and SiteForCookies on redirect.
    // These requests allow users to be recognized across sites on redirect, so
    // should not generally be used for anything other than navigations.
    kUpdateTopFrame,

    // Only update frame origin on redirect.
    kUpdateFrameOnly,

    // Update nothing on redirect.
    kUpdateNothing,
  };

  // Default constructor returns an IsolationInfo with empty origins, a null
  // SiteForCookies(), and a RedirectMode of kUpdateNothing.
  IsolationInfo();
  IsolationInfo(const IsolationInfo&);
  IsolationInfo(IsolationInfo&&);
  ~IsolationInfo();

  IsolationInfo& operator=(const IsolationInfo&);
  IsolationInfo& operator=(IsolationInfo&&);

  // Simple constructor for internal requests. Sets |frame_origin| and
  // |site_for_cookies| match |top_frame_origin|. Sets |redirect_mode| to
  // kUpdateNothing. Will only send SameSite cookies to the site associated with
  // the passed in origin.
  static IsolationInfo CreateForInternalRequest(
      const url::Origin& top_frame_origin);

  // Creates a transient IsolationInfo. A transient IsolationInfo will not save
  // data to disk and not send SameSite cookies. Equivalent to calling
  // CreateForInternalRequest with a fresh opaque origin.
  static IsolationInfo CreateTransient();

  // Creates a non-transient IsolationInfo. Just like a transient IsolationInfo
  // (no SameSite cookies, opaque Origins), but does write data to disk, so this
  // allows use of the disk cache with a transient NIK.
  static IsolationInfo CreateOpaqueAndNonTransient();

  // Creates an IsolationInfo with the provided parameters. If the parameters
  // are inconsistent, DCHECKs. In particular:
  // * If |redirect_mode| is kUpdateTopFrame, |top_frame_origin| must equal
  //   |frame_origin|, and |site_for_cookies| must be either null or first party
  //   with respect to them.
  // * If |redirect_mode| is kUpdateFrameOnly, |top_frame_origin| must be
  //   first party with respect to |site_for_cookies|, or |site_for_cookies|
  //   must be null.
  // * If |redirect_mode| is kUpdateNothing, |top_frame_origin| and
  //   |frame_origin| must be first party with respect to |site_for_cookies|, or
  //   |site_for_cookies| must be null.
  //
  // Note that the |site_for_cookies| consistency checks are skipped when
  // |site_for_cookies| is not HTTP/HTTPS.
  static IsolationInfo Create(RedirectMode redirect_mode,
                              const url::Origin& top_frame_origin,
                              const url::Origin& frame_origin,
                              const SiteForCookies& site_for_cookies);

  // Create an IsolationInfos that may not be fully correct - in particular,
  // the SiteForCookies will always set to null, and if the NetworkIsolationKey
  // only has a top frame origin, the frame origin will either be set to the top
  // frame origin, in the kUpdateTopFrame case, or be replaced by an opaque
  // origin in all other cases. If the NetworkIsolationKey is not fully
  // populated, will create an empty IsolationInfo. This is intended for use
  // while transitioning from NIKs being set on only some requests to
  // IsolationInfos being set on all requests.
  //
  // TODO(https://crbug.com/1060631): Remove this once no longer needed.
  static IsolationInfo CreatePartial(
      RedirectMode redirect_mode,
      const net::NetworkIsolationKey& network_isolation_key);

  // Returns nullopt if the arguments are not consistent. Otherwise, returns a
  // fully populated IsolationInfo. Any IsolationInfo that can be created by
  // the other construction methods, including the 0-argument constructor, is
  // considered consistent.
  //
  // Intended for use by cross-process deserialization.
  static base::Optional<IsolationInfo> CreateIfConsistent(
      RedirectMode redirect_mode,
      const base::Optional<url::Origin>& top_frame_origin,
      const base::Optional<url::Origin>& frame_origin,
      const SiteForCookies& site_for_cookies,
      bool opaque_and_non_transient);

  // Create a new IsolationInfo for a redirect to the supplied origin. |this| is
  // unmodified.
  IsolationInfo CreateForRedirect(const url::Origin& new_origin) const;

  RedirectMode redirect_mode() const { return redirect_mode_; }

  bool IsEmpty() const { return !top_frame_origin_; }

  // These may only be nullopt if created by the empty constructor. If one is
  // nullopt, both are, and SiteForCookies is null.
  const base::Optional<url::Origin>& top_frame_origin() const {
    return top_frame_origin_;
  }
  const base::Optional<url::Origin>& frame_origin() const {
    return frame_origin_;
  }

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  // The value that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  //
  // WARNING: This value must only be used for the third-party cookie blocking
  //          policy. It MUST NEVER be used for any kind of SECURITY check.
  const SiteForCookies& site_for_cookies() const { return site_for_cookies_; }

  bool opaque_and_non_transient() const { return opaque_and_non_transient_; }

  bool IsEqualForTesting(const IsolationInfo& other) const;

 private:
  IsolationInfo(RedirectMode redirect_mode,
                const base::Optional<url::Origin>& top_frame_origin,
                const base::Optional<url::Origin>& frame_origin,
                const SiteForCookies& site_for_cookies,
                bool opaque_and_non_transient);

  RedirectMode redirect_mode_;

  base::Optional<url::Origin> top_frame_origin_;
  base::Optional<url::Origin> frame_origin_;

  // This can be deduced from the two origins above, but keep a cached version
  // to avoid repeated eTLD+1 calculations, when this is using eTLD+1.
  net::NetworkIsolationKey network_isolation_key_;

  SiteForCookies site_for_cookies_;

  bool opaque_and_non_transient_ = false;
};

}  // namespace net

#endif  // NET_BASE_ISOLATION_INFO_H_
