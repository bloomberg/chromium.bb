// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_browser.h"

#include <utility>

#include "content/public/common/user_agent.h"
#include "headless/public/version.h"

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_types.h"
#endif

using Options = headless::HeadlessBrowser::Options;
using Builder = headless::HeadlessBrowser::Options::Builder;

namespace headless {

namespace {
// Product name for building the default user agent string.
const char kProductName[] = "HeadlessChrome";
constexpr gfx::Size kDefaultWindowSize(800, 600);

std::string GetProductNameAndVersion() {
  return std::string(kProductName) + "/" + PRODUCT_VERSION;
}
}  // namespace

Options::Options(int argc, const char** argv)
    : argc(argc),
      argv(argv),
#if defined(OS_WIN)
      instance(0),
      sandbox_info(nullptr),
#endif
      devtools_socket_fd(0),
      message_pump(nullptr),
      single_process_mode(false),
      disable_sandbox(false),
      enable_resource_scheduler(true),
#if defined(USE_OZONE)
      // TODO(skyostil): Implement SwiftShader backend for headless ozone.
      gl_implementation("osmesa"),
#elif defined(OS_WIN)
      // TODO(skyostil): Enable SwiftShader on Windows (crbug.com/729961).
      gl_implementation("osmesa"),
#elif !defined(OS_MACOSX)
      gl_implementation("swiftshader-webgl"),
#else
      gl_implementation("any"),
#endif
      product_name_and_version(GetProductNameAndVersion()),
      user_agent(content::BuildUserAgentFromProduct(product_name_and_version)),
      window_size(kDefaultWindowSize),
      incognito_mode(true),
      enable_crash_reporter(false) {
}

Options::Options(Options&& options) = default;

Options::~Options() {}

Options& Options::operator=(Options&& options) = default;

bool Options::DevtoolsServerEnabled() {
  return (devtools_endpoint.address().IsValid() || devtools_socket_fd != 0);
}

Builder::Builder(int argc, const char** argv) : options_(argc, argv) {}

Builder::Builder() : options_(0, nullptr) {}

Builder::~Builder() {}

Builder& Builder::SetProductNameAndVersion(
    const std::string& product_name_and_version) {
  options_.product_name_and_version = product_name_and_version;
  return *this;
}

Builder& Builder::SetUserAgent(const std::string& user_agent) {
  options_.user_agent = user_agent;
  return *this;
}

Builder& Builder::SetAcceptLanguage(const std::string& accept_language) {
  options_.accept_language = accept_language;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const net::IPEndPoint& endpoint) {
  options_.devtools_endpoint = endpoint;
  return *this;
}

Builder& Builder::EnableDevToolsServer(const size_t socket_fd) {
  options_.devtools_socket_fd = socket_fd;
  return *this;
}

Builder& Builder::SetMessagePump(base::MessagePump* message_pump) {
  options_.message_pump = message_pump;
  return *this;
}

Builder& Builder::SetProxyConfig(
    std::unique_ptr<net::ProxyConfig> proxy_config) {
  options_.proxy_config = std::move(proxy_config);
  return *this;
}

Builder& Builder::SetHostResolverRules(const std::string& host_resolver_rules) {
  options_.host_resolver_rules = host_resolver_rules;
  return *this;
}

Builder& Builder::SetSingleProcessMode(bool single_process_mode) {
  options_.single_process_mode = single_process_mode;
  return *this;
}

Builder& Builder::SetDisableSandbox(bool disable_sandbox) {
  options_.disable_sandbox = disable_sandbox;
  return *this;
}

Builder& Builder::SetEnableResourceScheduler(bool enable_resource_scheduler) {
  options_.enable_resource_scheduler = enable_resource_scheduler;
  return *this;
}

Builder& Builder::SetGLImplementation(const std::string& gl_implementation) {
  options_.gl_implementation = gl_implementation;
  return *this;
}

Builder& Builder::AddMojoServiceName(const std::string& mojo_service_name) {
  options_.mojo_service_names.insert(mojo_service_name);
  return *this;
}

#if defined(OS_WIN)
Builder& Builder::SetInstance(HINSTANCE instance) {
  options_.instance = instance;
  return *this;
}

Builder& Builder::SetSandboxInfo(sandbox::SandboxInterfaceInfo* sandbox_info) {
  options_.sandbox_info = sandbox_info;
  return *this;
}
#endif  // defined(OS_WIN)

Builder& Builder::SetUserDataDir(const base::FilePath& user_data_dir) {
  options_.user_data_dir = user_data_dir;
  return *this;
}

Builder& Builder::SetWindowSize(const gfx::Size& window_size) {
  options_.window_size = window_size;
  return *this;
}

Builder& Builder::SetIncognitoMode(bool incognito_mode) {
  options_.incognito_mode = incognito_mode;
  return *this;
}

Builder& Builder::SetOverrideWebPreferencesCallback(
    base::Callback<void(WebPreferences*)> callback) {
  options_.override_web_preferences_callback = callback;
  return *this;
}

Builder& Builder::SetCrashReporterEnabled(bool enabled) {
  options_.enable_crash_reporter = enabled;
  return *this;
}

Builder& Builder::SetCrashDumpsDir(const base::FilePath& dir) {
  options_.crash_dumps_dir = dir;
  return *this;
}

Options Builder::Build() {
  return std::move(options_);
}

}  // namespace headless
