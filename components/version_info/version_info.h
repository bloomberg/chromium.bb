// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_INFO_VERSION_INFO_H_
#define COMPONENTS_VERSION_INFO_VERSION_INFO_H_

#include <string>

#include "components/version_info/channel.h"

namespace base {
class Version;
}

namespace version_info {

// Returns the product name and version information for UserAgent header,
// e.g. "Chrome/a.b.c.d".
std::string GetProductNameAndVersionForUserAgent();

// Returns the product name, e.g. "Chromium" or "Google Chrome".
std::string GetProductName();

// Returns the version number, e.g. "6.0.490.1".
std::string GetVersionNumber();

// Returns the result of GetVersionNumber() as a base::Version.
const base::Version& GetVersion();

// Returns a version control specific identifier of this release.
std::string GetLastChange();

// Returns whether this is an "official" release of the current version, i.e.
// whether kwnowing GetVersionNumber() is enough to completely determine what
// GetLastChange() is.
bool IsOfficialBuild();

// Returns the OS type, e.g. "Windows", "Linux", "FreeBDS", ...
std::string GetOSType();

// Returns whether SetChannel() has been called or not.
bool IsChannelSet();

// Must be called before calling GetChannel(). Subsequent calls are no-ops.
// Ideally, there should be only one caller to SetChannel(). In situations where
// there are multiple callers, then the first caller "wins". To prevent
// inconsistencies with GetChannel(), all the callers should pass in the same
// |channel| value.
void SetChannel(Channel channel);

// Gets the channel. DCHECKs if SetChannel() never got called.
Channel GetChannel();

// Returns a string equivalent of |channel|, indenpendent of whether the build
// is branded or not and without any additional modifiers.
std::string GetChannelString(Channel channel);

}  // namespace version_info

#endif  // COMPONENTS_VERSION_INFO_VERSION_INFO_H_
