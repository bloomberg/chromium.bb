// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TRUST_STORE_H_
#define NET_CERT_INTERNAL_TRUST_STORE_H_

#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/cert/internal/parsed_certificate.h"

namespace net {

namespace der {
class Input;
}

// A TrustAnchor represents a trust anchor used during RFC 5280 path validation.
//
// At its core, each trust anchor has two parts:
//  * Name
//  * Public Key
//
// Optionally a trust anchor may contain:
//  * An associated certificate
//  * Trust anchor constraints
//
// Relationship between ParsedCertificate and TrustAnchor:
//
// For convenience trust anchors are often described using a
// (self-signed) certificate. TrustAnchor facilitates this by allowing
// construction of a TrustAnchor given a ParsedCertificate, however
// the concepts are NOT quite the same.
//
// Notably when constructed from a certificate, properties/constraints of
// the underlying certificate like expiration, signature, or basic
// constraints are NOT processed and validated by path validation.
// Instead such properties need to be explicitly indicated via "trust
// anchor constraints".
//
// See RFC 5937 and RFC 5280 for more details.
class NET_EXPORT TrustAnchor : public base::RefCountedThreadSafe<TrustAnchor> {
 public:
  // Creates a TrustAnchor given a certificate. The only parts of the
  // certificate that will be used are the subject and SPKI. Any extensions in
  // the certificate that might limit its use (like name constraints or policy)
  // are disregarded during validation. In other words, the resulting trust
  // anchor has no anchor constraints.
  static scoped_refptr<TrustAnchor> CreateFromCertificateNoConstraints(
      scoped_refptr<ParsedCertificate> cert);

  // TODO(crbug.com/635200): Support anchor constraints. For instance
  // by adding factory method CreateFromCertificateWithConstraints()

  der::Input spki() const;
  der::Input normalized_subject() const;

  // Returns the optional certificate representing this trust anchor.
  // In the current implementation it will never return nullptr...
  // however clients should be prepared to handle this case.
  const scoped_refptr<ParsedCertificate>& cert() const;

 private:
  friend class base::RefCountedThreadSafe<TrustAnchor>;
  explicit TrustAnchor(scoped_refptr<ParsedCertificate>);
  ~TrustAnchor();

  scoped_refptr<ParsedCertificate> cert_;
};

using TrustAnchors = std::vector<scoped_refptr<TrustAnchor>>;

// A very simple implementation of a TrustStore, which contains a set of
// trust anchors.
//
// TODO(mattm): convert this into an interface, provide implementations that
// interface with OS trust store.
class NET_EXPORT TrustStore {
 public:
  TrustStore();
  ~TrustStore();

  // Empties the trust store, resetting it to original state.
  void Clear();

  void AddTrustAnchor(scoped_refptr<TrustAnchor> anchor);

  // Returns the trust anchors that match |name| in |*matches|, if any.
  void FindTrustAnchorsByNormalizedName(const der::Input& normalized_name,
                                        TrustAnchors* matches) const;

 private:
  // Multimap from normalized subject -> TrustAnchor.
  std::unordered_multimap<base::StringPiece,
                          scoped_refptr<TrustAnchor>,
                          base::StringPieceHash>
      anchors_;

  DISALLOW_COPY_AND_ASSIGN(TrustStore);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_TRUST_STORE_H_
