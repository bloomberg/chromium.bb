// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_WHISPERNET_CLIENT_H_
#define COMPONENTS_COPRESENCE_PUBLIC_WHISPERNET_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

// The interface that the whispernet client needs to implement. These methods
// provide us the ability to use the audio medium in copresence. Currently since
// the only medium that copresence uses is audio, the implementation of this
// interface is required.
class WhispernetClient {
 public:
  // Generic callback to indicate a boolean success or failure.
  typedef base::Callback<void(bool)> SuccessCallback;
  // Callback that returns detected tokens.
  typedef base::Callback<void(const std::vector<std::string>&)> TokensCallback;
  // Callback that returns encoded samples for a given token.
  typedef base::Callback<
      void(const std::string&, const scoped_refptr<media::AudioBusRefCounted>&)>
      SamplesCallback;

  // Initialize the whispernet client and call the callback when done. The
  // parameter indicates whether we succeeded or failed.
  virtual void Initialize(const SuccessCallback& init_callback) = 0;
  // Copresence will call this before making any calls to its destructors.
  virtual void Shutdown() = 0;

  // Fires an event to request a token encode.
  virtual void EncodeToken(const std::string& token, bool audible) = 0;
  // Fires an event to request a decode for the given samples.
  virtual void DecodeSamples(const std::string& samples) = 0;
  // Fires an event to request detection of a whispernet broadcast.
  virtual void DetectBroadcast() = 0;

  // Callback registreation methods. These are the callbacks that will be
  // registered by Copresence to receive data.
  virtual void RegisterTokensCallback(
      const TokensCallback& tokens_callback) = 0;
  virtual void RegisterSamplesCallback(
      const SamplesCallback& samples_callback) = 0;
  virtual void RegisterDetectBroadcastCallback(
      const SuccessCallback& db_callback) = 0;

  // Don't cache these callbacks, as they may become invalid at any time.
  // Always invoke callbacks directly through these accessors.
  virtual TokensCallback GetTokensCallback() = 0;
  virtual SamplesCallback GetSamplesCallback() = 0;
  virtual SuccessCallback GetDetectBroadcastCallback() = 0;
  virtual SuccessCallback GetInitializedCallback() = 0;

  virtual ~WhispernetClient() {};
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_WHISPERNET_CLIENT_H_
