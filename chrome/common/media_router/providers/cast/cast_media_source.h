// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_
#define CHROME_COMMON_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/common/media_router/media_source.h"
#include "components/cast_channel/cast_message_util.h"
#include "components/cast_channel/cast_socket.h"

namespace media_router {

static constexpr char kCastStreamingAppId[] = "0F5096E8";
static constexpr char kCastStreamingAudioAppId[] = "85CDB22F";

// Placeholder app ID advertised by the multizone leader in a receiver status
// message.
static constexpr char kMultizoneLeaderAppId[] = "MultizoneLeader";

static constexpr base::TimeDelta kDefaultLaunchTimeout =
    base::TimeDelta::FromSeconds(60);

// Represents a Cast app and its capabilitity requirements.
struct CastAppInfo {
  explicit CastAppInfo(const std::string& app_id);
  ~CastAppInfo();

  CastAppInfo(const CastAppInfo& other);

  std::string app_id;

  // A bitset of capabilities required by the app.
  int required_capabilities = cast_channel::CastDeviceCapability::NONE;
};

// Auto-join policy determines when the SDK will automatically connect a sender
// application to an existing session after API initialization.
enum class AutoJoinPolicy {
  // Automatically connects when the session was started with the same app ID,
  // in the same tab and page origin.
  kTabAndOriginScoped,
  // Automatically connects when the session was started with the same app ID
  // and the same page origin (regardless of tab).
  kOriginScoped,
  // No automatic connection.
  kPageScoped,
  // No policy was specified.  Generally treated the same as kPageScoped.
  kNone,
};

// Represents a MediaSource parsed into structured, Cast specific data. The
// following MediaSources can be parsed into CastMediaSource:
// - Cast Presentation URLs
// - HTTP(S) Presentation URLs
// - Desktop / tab mirroring URNs
class CastMediaSource {
 public:
  // Returns the parsed form of |source|, or nullptr if it cannot be parsed.
  static std::unique_ptr<CastMediaSource> FromMediaSourceId(
      const MediaSource::Id& source);

  static std::unique_ptr<CastMediaSource> FromAppId(const std::string& app_id);

  CastMediaSource(const MediaSource::Id& source_id,
                  const std::vector<CastAppInfo>& app_infos,
                  AutoJoinPolicy auto_join_policy = AutoJoinPolicy::kNone);
  CastMediaSource(const CastMediaSource& other);
  ~CastMediaSource();

  // Returns |true| if |app_infos| contain |app_id|.
  bool ContainsApp(const std::string& app_id) const;
  bool ContainsAnyAppFrom(const std::vector<std::string>& app_ids) const;

  // Returns a list of App IDs in this CastMediaSource.
  std::vector<std::string> GetAppIds() const;

  const MediaSource::Id& source_id() const { return source_id_; }
  const std::vector<CastAppInfo>& app_infos() const { return app_infos_; }
  const std::string& client_id() const { return client_id_; }
  void set_client_id(const std::string& client_id) { client_id_ = client_id; }
  base::TimeDelta launch_timeout() const { return launch_timeout_; }
  void set_launch_timeout(base::TimeDelta launch_timeout) {
    launch_timeout_ = launch_timeout;
  }
  const base::Optional<cast_channel::BroadcastRequest>& broadcast_request()
      const {
    return broadcast_request_;
  }

  void set_broadcast_request(const cast_channel::BroadcastRequest& request) {
    broadcast_request_ = request;
  }

  AutoJoinPolicy auto_join_policy() const { return auto_join_policy_; }

 private:
  MediaSource::Id source_id_;
  std::vector<CastAppInfo> app_infos_;
  AutoJoinPolicy auto_join_policy_;
  base::TimeDelta launch_timeout_ = kDefaultLaunchTimeout;
  // Empty if not set.
  std::string client_id_;
  base::Optional<cast_channel::BroadcastRequest> broadcast_request_;
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_PROVIDERS_CAST_CAST_MEDIA_SOURCE_H_
