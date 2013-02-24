// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransportSecurityState maintains an in memory database containing the
// list of hosts that currently have transport security enabled. This
// singleton object deals with writing that data out to disk as needed and
// loading it at startup.

// At startup we need to load the transport security state from the
// disk. For the moment, we don't want to delay startup for this load, so we
// let the TransportSecurityState run for a while without being loaded.
// This means that it's possible for pages opened very quickly not to get the
// correct transport security information.
//
// To load the state, we schedule a Task on the file thread which loads,
// deserializes and configures the TransportSecurityState.
//
// The TransportSecurityState object supports running a callback function
// when it changes. This object registers the callback, pointing at itself.
//
// TransportSecurityState calls...
// TransportSecurityPersister::StateIsDirty
//   since the callback isn't allowed to block or reenter, we schedule a Task
//   on the file thread after some small amount of time
//
// ...
//
// TransportSecurityPersister::SerializeState
//   copies the current state of the TransportSecurityState, serializes
//   and writes to disk.

#ifndef CHROME_BROWSER_NET_TRANSPORT_SECURITY_PERSISTER_H_
#define CHROME_BROWSER_NET_TRANSPORT_SECURITY_PERSISTER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/weak_ptr.h"
#include "net/base/transport_security_state.h"

// Reads and updates on-disk TransportSecurity state.
// Must be created, used and destroyed only on the IO thread.
class TransportSecurityPersister
    : public net::TransportSecurityState::Delegate,
      public base::ImportantFileWriter::DataSerializer {
 public:
  TransportSecurityPersister(net::TransportSecurityState* state,
                             const base::FilePath& profile_path,
                             bool readonly);
  virtual ~TransportSecurityPersister();

  // Called by the TransportSecurityState when it changes its state.
  virtual void StateIsDirty(net::TransportSecurityState*) OVERRIDE;

  // ImportantFileWriter::DataSerializer:
  //
  // Serializes |transport_security_state_| into |*output|. Returns true if
  // all DomainStates were serialized correctly.
  //
  // The serialization format is JSON; the JSON represents a dictionary of
  // host:DomainState pairs (host is a string). The DomainState is
  // represented as a dictionary containing the following keys and value
  // types (not all keys will always be present):
  //
  //     "include_subdomains": true|false
  //     "created": double
  //     "expiry": double
  //     "dynamic_spki_hashes_expiry": double
  //     "mode": "default"|"force-https"
  //             legacy value synonyms "strict" = "force-https"
  //                                   "pinning-only" = "default"
  //             legacy value "spdy-only" is unused and ignored
  //     "static_spki_hashes": list of strings
  //         legacy key synonym "preloaded_spki_hashes"
  //     "bad_static_spki_hashes": list of strings
  //         legacy key synonym "bad_preloaded_spki_hashes"
  //     "dynamic_spki_hashes": list of strings
  //
  // The JSON dictionary keys are strings containing
  // Base64(SHA256(net::TransportSecurityState::CanonicalizeHost(domain))).
  // The reason for hashing them is so that the stored state does not
  // trivially reveal a user's browsing history to an attacker reading the
  // serialized state on disk.
  virtual bool SerializeData(std::string* data) OVERRIDE;

  // Parses an array of JSON-encoded TransportSecurityState::DomainState
  // entries. For use in loading entries defined on the command line
  // (switches::kHstsHosts).
  bool DeserializeFromCommandLine(const std::string& serialized);

  // Clears any existing non-static entries, and then re-populates
  // |transport_security_state_|.
  //
  // Sets |*dirty| to true if the new state differs from the persisted
  // state; false otherwise.
  bool LoadEntries(const std::string& serialized, bool* dirty);

 private:
  class Loader;

  // Populates |state| from the JSON string |serialized|. Returns true if
  // all entries were parsed and deserialized correctly. If |forced| is
  // true, updates |state|'s map of "forced" DomainState entries; normally,
  // leave this false.
  //
  // Sets |*dirty| to true if the new state differs from the persisted
  // state; false otherwise.
  static bool Deserialize(const std::string& serialized,
                          bool forced,
                          bool* dirty,
                          net::TransportSecurityState* state);

  void CompleteLoad(const std::string& state);

  net::TransportSecurityState* transport_security_state_;

  // Helper for safely writing the data.
  base::ImportantFileWriter writer_;

  // Whether or not we're in read-only mode.
  const bool readonly_;

  base::WeakPtrFactory<TransportSecurityPersister> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TransportSecurityPersister);
};

#endif  // CHROME_BROWSER_NET_TRANSPORT_SECURITY_PERSISTER_H_
