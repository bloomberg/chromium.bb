// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_CONFIGURATION_H_
#define CHROME_INSTALLER_MINI_INSTALLER_CONFIGURATION_H_

namespace mini_installer {

// A simple container of the mini_installer's configuration, as dictated by the
// command line used to invoke it.
class Configuration {
 public:
  enum Operation {
    INSTALL_PRODUCT,
    CLEANUP,
  };

  Configuration();
  ~Configuration();

  // Initializes this instance on the basis of the process's command line.
  bool Initialize();

  // Returns the desired operation dictated by the command line options.
  Operation operation() const { return operation_; }

  // Returns the program portion of the command line, or NULL if it cannot be
  // determined (e.g., by misuse).
  const wchar_t* program() const;

  // Returns the number of arguments specified on the command line, including
  // the program itself.
  int argument_count() const { return argument_count_; }

  // Returns the original command line.
  const wchar_t* command_line() const { return command_line_; }

  // Returns the app guid to be used for Chrome.  --chrome-sxs on the command
  // line makes this the canary's app guid.
  const wchar_t* chrome_app_guid() const { return chrome_app_guid_; }

  // Returns true if --chrome is explicitly or implicitly on the command line.
  bool has_chrome() const { return has_chrome_; }

  // Returns true if --chrome-frame is on the command line.
  bool has_chrome_frame() const { return has_chrome_frame_; }

  // Returns true if --app-host is on the command line.
  bool has_app_host() const { return has_app_host_; }

  // Returns true if --multi-install is on the command line.
  bool is_multi_install() const { return is_multi_install_; }

  // Returns true if --system-level is on the command line.
  bool is_system_level() const { return is_system_level_; }

  // Returns true if --query-component-build is on the command line.
  // This will cause mini_installer to exit and return 1 if this is
  // a component install or 0 otherwise.
  bool query_component_build() const { return query_component_build_; }

 protected:
  void Clear();
  bool InitializeFromCommandLine(const wchar_t* command_line);

  wchar_t** args_;
  const wchar_t* chrome_app_guid_;
  const wchar_t* command_line_;
  int argument_count_;
  Operation operation_;
  bool has_chrome_;
  bool has_chrome_frame_;
  bool has_app_host_;
  bool is_multi_install_;
  bool is_system_level_;
  bool query_component_build_;

 private:
  Configuration(const Configuration&);
  Configuration& operator=(const Configuration&);
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_CONFIGURATION_H_
