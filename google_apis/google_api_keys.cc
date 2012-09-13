// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/google_api_keys.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringize_macros.h"

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_OFFICIAL_GOOGLE_API_KEYS)
#include "google_apis/internal/google_chrome_api_keys.h"
#endif

// Used to indicate an unset key/id/secret.  This works better with
// various unit tests than leaving the token empty.
#define DUMMY_API_TOKEN "dummytoken"

#if !defined(GOOGLE_API_KEY)
#define GOOGLE_API_KEY DUMMY_API_TOKEN
#endif

#if !defined(GOOGLE_CLIENT_ID_MAIN)
// TODO(joi): Use DUMMY_API_TOKEN here once folks on chromium-dev have
// had a couple of days to update their include.gypi.
#define GOOGLE_CLIENT_ID_MAIN "77185425430.apps.googleusercontent.com"
#endif

#if !defined(GOOGLE_CLIENT_SECRET_MAIN)
// TODO(joi): As above.
#define GOOGLE_CLIENT_SECRET_MAIN "OTJgUOQcT7lO7GsGZq2G4IlT"
#endif

#if !defined(GOOGLE_CLIENT_ID_CLOUD_PRINT)
// TODO(joi): As above.
#define GOOGLE_CLIENT_ID_CLOUD_PRINT "551556820943.apps.googleusercontent.com"
#endif

#if !defined(GOOGLE_CLIENT_SECRET_CLOUD_PRINT)
// TODO(joi): As above.
#define GOOGLE_CLIENT_SECRET_CLOUD_PRINT "u3/mp8CgLFxh4uiX1855/MHe"
#endif

#if !defined(GOOGLE_CLIENT_ID_REMOTING)
// TODO(joi): As above.
#define GOOGLE_CLIENT_ID_REMOTING \
  "440925447803-avn2sj1kc099s0r7v62je5s339mu0am1.apps.googleusercontent.com"
#endif

#if !defined(GOOGLE_CLIENT_SECRET_REMOTING)
// TODO(joi): As above.
#define GOOGLE_CLIENT_SECRET_REMOTING "Bgur6DFiOMM1h8x-AQpuTQlK"
#endif

// These are used as shortcuts for developers and users providing
// OAuth credentials via preprocessor defines or environment
// variables.  If set, they will be used to replace any of the client
// IDs and secrets above that have not been set (and only those; they
// will not override already-set values).
#if !defined(GOOGLE_DEFAULT_CLIENT_ID)
#define GOOGLE_DEFAULT_CLIENT_ID ""
#endif
#if !defined(GOOGLE_DEFAULT_CLIENT_SECRET)
#define GOOGLE_DEFAULT_CLIENT_SECRET ""
#endif

namespace switches {

// Specifies custom OAuth2 client id for testing purposes.
const char kOAuth2ClientID[] = "oauth2-client-id";

// Specifies custom OAuth2 client secret for testing purposes.
const char kOAuth2ClientSecret[] = "oauth2-client-secret";

}  // namespace switches

namespace google_apis {

namespace {

// This is used as a lazy instance to determine keys once and cache them.
class APIKeyCache {
 public:
  APIKeyCache() {
    scoped_ptr<base::Environment> environment(base::Environment::Create());
    CommandLine* command_line = CommandLine::ForCurrentProcess();

    api_key_ = CalculateKeyValue(GOOGLE_API_KEY,
                                 STRINGIZE_NO_EXPANSION(GOOGLE_API_KEY),
                                 NULL, "",
                                 environment.get(),
                                 command_line);

    std::string default_client_id = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_ID,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_ID),
        NULL, "",
        environment.get(),
        command_line);
    std::string default_client_secret = CalculateKeyValue(
        GOOGLE_DEFAULT_CLIENT_SECRET,
        STRINGIZE_NO_EXPANSION(GOOGLE_DEFAULT_CLIENT_SECRET),
        NULL, "",
        environment.get(),
        command_line);

    // We currently only allow overriding the baked-in values for the
    // default OAuth2 client ID and secret using a command-line
    // argument, since that is useful to enable testing against
    // staging servers, and since that was what was possible and
    // likely practiced by the QA team before this implementation was
    // written.
    client_ids_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_MAIN,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_MAIN),
        switches::kOAuth2ClientID,
        default_client_id,
        environment.get(),
        command_line);
    client_secrets_[CLIENT_MAIN] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_MAIN,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_MAIN),
        switches::kOAuth2ClientSecret,
        default_client_secret,
        environment.get(),
        command_line);

    client_ids_[CLIENT_CLOUD_PRINT] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_CLOUD_PRINT,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_CLOUD_PRINT),
        NULL,
        default_client_id,
        environment.get(),
        command_line);
    client_secrets_[CLIENT_CLOUD_PRINT] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_CLOUD_PRINT,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_CLOUD_PRINT),
        NULL,
        default_client_secret,
        environment.get(),
        command_line);

    client_ids_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_ID_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_ID_REMOTING),
        NULL,
        default_client_id,
        environment.get(),
        command_line);
    client_secrets_[CLIENT_REMOTING] = CalculateKeyValue(
        GOOGLE_CLIENT_SECRET_REMOTING,
        STRINGIZE_NO_EXPANSION(GOOGLE_CLIENT_SECRET_REMOTING),
        NULL,
        default_client_secret,
        environment.get(),
        command_line);
  }

  std::string api_key() const { return api_key_; }

  std::string GetClientID(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_ids_[client];
  }

  std::string GetClientSecret(OAuth2Client client) const {
    DCHECK_LT(client, CLIENT_NUM_ITEMS);
    return client_secrets_[client];
  }

 private:
  // Gets a value for a key.  In priority order, this will be the value
  // provided via a command-line switch, the value provided via an
  // environment variable, or finally a value baked into the build.
  // |command_line_switch| may be NULL.
  static std::string CalculateKeyValue(const char* baked_in_value,
                                       const char* environment_variable_name,
                                       const char* command_line_switch,
                                       const std::string& default_if_unset,
                                       base::Environment* environment,
                                       CommandLine* command_line) {
    std::string key_value = baked_in_value;
    std::string temp;
    if (environment->GetVar(environment_variable_name, &temp)) {
      key_value = temp;
      LOG(INFO) << "Overriding API key " << environment_variable_name
                << " with value " << key_value << " from environment variable.";
    }

    if (command_line_switch && command_line->HasSwitch(command_line_switch)) {
      key_value = command_line->GetSwitchValueASCII(command_line_switch);
      LOG(INFO) << "Overriding API key " << environment_variable_name
                << " with value " << key_value << " from command-line switch.";
    }

    if (key_value.size() == 0) {
#if defined(GOOGLE_CHROME_BUILD)
      // No key should be empty in an official build, except the
      // default keys themselves, which will have an empty default.
      CHECK(default_if_unset.size() == 0);
#endif
      LOG(INFO) << "Using default value \"" << default_if_unset
                << "\" for API key " << environment_variable_name;
      key_value = default_if_unset;
    }

    // This should remain a debug-only log.
    DVLOG(1) << "API key " << environment_variable_name << "=" << key_value;

    return key_value;
  }

  std::string api_key_;
  std::string client_ids_[CLIENT_NUM_ITEMS];
  std::string client_secrets_[CLIENT_NUM_ITEMS];
};

static base::LazyInstance<APIKeyCache> g_api_key_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

std::string GetAPIKey() {
  return g_api_key_cache.Get().api_key();
}

std::string GetOAuth2ClientID(OAuth2Client client) {
  return g_api_key_cache.Get().GetClientID(client);
}

std::string GetOAuth2ClientSecret(OAuth2Client client) {
  return g_api_key_cache.Get().GetClientSecret(client);
}

}  // namespace google_apis
