// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_
#define CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_

#include <memory>
#include <string>

#include "chrome/install_static/install_constants.h"

namespace install_static {

class PrimaryInstallDetails;

// Details relating to how Chrome is installed. This class and
// PrimaryInstallDetails (below) are used in tandem so that one instance of the
// latter may be initialized early during process startup and then shared with
// other modules in the process. For example, chrome_elf creates the instance
// for a Chrome process and exports a GetInstallDetailsPayload function used by
// chrome.exe and chrome.dll to create their own module-specific instances
// referring to the same underlying payload. See install_modes.h for a gentle
// introduction to such terms as "brand" and "mode".
class InstallDetails {
 public:
  // A POD-struct containing the underlying data for an InstallDetails
  // instance. Multiple InstallDetails instances (e.g., one per module in a
  // process) share a single underlying Payload.
  struct Payload {
    // The size (in bytes) of this structure. This serves to verify that all
    // modules in a process have the same definition of the struct.
    size_t size;

    // The compile-time version of the product at the time that the process's
    // primary module was built. This is used to detect version skew across
    // modules in the process.
    const char* product_version;

    // The brand-specific install mode for this install; see kInstallModes.
    const InstallConstants* mode;

    // The friendly name of this Chrome's channel, or an empty string if the
    // brand does not integrate with Google Update.
    const wchar_t* channel;

    // The string length of |channel| (not including the string terminator).
    size_t channel_length;

    // True if installed in C:\Program Files{, {x86)}; otherwise, false.
    bool system_level;

    // True if multi-install.
    bool multi_install;
  };

  InstallDetails(const InstallDetails&) = delete;
  InstallDetails(InstallDetails&&) = delete;
  InstallDetails& operator=(const InstallDetails&) = delete;
  virtual ~InstallDetails() = default;

  // Returns the instance for this module.
  static const InstallDetails& Get();

  // This mode's index into the brand's array of install modes. This will match
  // a brand-specific InstallConstantIndex enumerator.
  int install_mode_index() const { return payload_->mode->index; }

  // The mode's install suffix (e.g., " SxS" for canary Chrome), or an empty
  // string for a brand's primary install mode.
  const wchar_t* install_suffix() const {
    return payload_->mode->install_suffix;
  }

  const InstallConstants& mode() const { return *payload_->mode; }

  // The app GUID with which this mode is registered with Google Update, or an
  // empty string if this brand does not integrate with Google Update.
  const wchar_t* app_guid() const { return payload_->mode->app_guid; }

  // True if the mode supports installation at system-level.
  bool supports_system_level() const {
    return payload_->mode->supports_system_level;
  }

  // True if the mode supports multi-install.
  bool supports_multi_install() const {
    return payload_->mode->supports_multi_install;
  }

  // The install's update channel, or an empty string if the brand does not
  // integrate with Google Update.
  std::wstring channel() const {
    return std::wstring(payload_->channel, payload_->channel_length);
  }
  bool system_level() const { return payload_->system_level; }
  bool multi_install() const { return payload_->multi_install; }

  // Returns the path to the installation's ClientState registry key. Returns
  // the path for the binaries if |binaries| and Chrome is
  // multi-install. Otherwise, returns the path for Chrome itself. This registry
  // key is used to hold various installation-related values, including an
  // indication of consent for usage stats.
  std::wstring GetClientStateKeyPath(bool binaries) const;

  // Returns the path to the installation's ClientStateMedium registry key.
  // Returns the path for the binaries if |binaries| and Chrome is
  // multi-install. Otherwise, returns the path for Chrome itself. This
  // registry key is used to hold various installation-related values, including
  // an indication of consent for usage stats for a system-level install.
  std::wstring GetClientStateMediumKeyPath(bool binaries) const;

  // Returns true if there is an indication of a mismatch between the primary
  // module and this module.
  bool VersionMismatch() const;

  // Sets the instance for the process. This must be called only once per
  // process during startup.
  static void SetForProcess(std::unique_ptr<PrimaryInstallDetails> details);

  // Returns a pointer to the module's payload so that it may be shared with
  // other modules in the process.
  static const Payload* GetPayload();

  // Initializes this module's instance with the payload owned by the process's
  // primary module (the one that used SetForProcess). Said primary module must
  // export the function:
  // extern "C" const install_static::InstallDetails::Payload*
  // GetInstallDetailsPayload();
  static void InitializeFromPrimaryModule(const wchar_t* primary_module_name);

 protected:
  explicit InstallDetails(const Payload* payload) : payload_(payload) {}
  const wchar_t* default_channel_name() const {
    return payload_->mode->default_channel_name;
  }

 private:
  const Payload* const payload_;
};

// A kind of InstallDetails that owns its payload. A single instance of this
// class is initialized early on in process startup (e.g., in chrome_elf for the
// case of chrome.exe; see InitializeProductDetailsForPrimaryModule). Its
// underlying data (its "payload") is shared with other interested modules in
// the process.
class PrimaryInstallDetails : public InstallDetails {
 public:
  PrimaryInstallDetails();
  PrimaryInstallDetails(const PrimaryInstallDetails&) = delete;
  PrimaryInstallDetails(PrimaryInstallDetails&&) = delete;
  PrimaryInstallDetails& operator=(const PrimaryInstallDetails&) = delete;

  void set_mode(const InstallConstants* mode) { payload_.mode = mode; }
  void set_channel(const std::wstring& channel) {
    channel_ = channel;
    payload_.channel = channel_.c_str();
    payload_.channel_length = channel_.size();
  }
  void set_system_level(bool system_level) {
    payload_.system_level = system_level;
  }
  void set_multi_install(bool multi_install) {
    payload_.multi_install = multi_install;
  }

 private:
  std::wstring channel_;
  Payload payload_ = Payload();
};

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_INSTALL_DETAILS_H_
