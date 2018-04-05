// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_

class PrefRegistrySimple;

namespace certificate_transparency {
namespace prefs {

// Registers the preferences related to Certificate Transparency policy
// in the given pref registry.
void RegisterPrefs(PrefRegistrySimple* registry);

// The set of hosts (as URLBlacklist-syntax filters) for which Certificate
// Transparency is required to be present.
extern const char kCTRequiredHosts[];

// The set of hosts (as URLBlacklist-syntax filters) for which Certificate
// Transparency information is allowed to be absent, even if it would
// otherwise be required (e.g. as part of security policy).
extern const char kCTExcludedHosts[];

// The set of subjectPublicKeyInfo hashes in the form of
// <hash-name>"/"<base64-hash-value>. If a certificate matches this SPKI, then
// Certificate Transparency information is allowed to be absent if one of the
// following conditions are met:
// 1) The matching certificate is a CA certificate (basicConstraints CA:TRUE)
//    that has a nameConstraints extension with a permittedSubtrees that
//    contains one or more directoryName entries, the directoryName has
//    one or more organizationName attributes, and the leaf certificate also
//    contains one or more organizationName attributes in the Subject.
// 2) The matching certificate contains one or more organizationName
//    attributes in the Subject, and those attributes are identical in
//    ordering, number of values, and byte-for-byte equality of values.
extern const char kCTExcludedSPKIs[];

// The set of subjectPublicKeyInfo hashes in the form of
// <hash-name>"/"<base64-hash-value>. If a certificate matches this SPKI, then
// Certificate Transparency information is allowed to be absent if:
// 1) The SPKI listed is a known as a publicly trusted root
//    (see //net/data/ssl/root_stores)
// 2) The SPKI listed is not actively trusted in the current version of the
//    ChromiumOS or Android root stores.
//    (see '"legacy": true' in root_stores.json)
extern const char kCTExcludedLegacySPKIs[];

}  // namespace prefs
}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_PREF_NAMES_H_
