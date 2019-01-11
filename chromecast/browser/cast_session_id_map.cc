// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_session_id_map.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"

namespace chromecast {
namespace shell {

// A small class that listens for the destruction of a WebContents, and forwards
// the event to the CastSessionIdMap with the appropriate group_id.
class CastSessionIdMap::GroupObserver : content::WebContentsObserver {
 public:
  using GroupDestroyedCallback =
      base::OnceCallback<void(base::UnguessableToken)>;

  GroupObserver(content::WebContents* web_contents,
                GroupDestroyedCallback destroyed_callback)
      : destroyed_callback_(std::move(destroyed_callback)),
        group_id_(web_contents->GetAudioGroupId()) {
    content::WebContentsObserver::Observe(web_contents);
  }

 private:
  // content::WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    DCHECK(destroyed_callback_);
    content::WebContentsObserver::Observe(nullptr);
    std::move(destroyed_callback_).Run(group_id_);
  }

  GroupDestroyedCallback destroyed_callback_;
  base::UnguessableToken group_id_;
};

// static
CastSessionIdMap* CastSessionIdMap::GetInstance(
    base::SequencedTaskRunner* task_runner) {
  static base::NoDestructor<CastSessionIdMap> map(task_runner);
  return map.get();
}

// static
void CastSessionIdMap::SetSessionId(std::string session_id,
                                    content::WebContents* web_contents) {
  base::UnguessableToken group_id = web_contents->GetAudioGroupId();
  GetInstance()->SetSessionIdInternal(session_id, group_id, web_contents);
}

// static
std::string CastSessionIdMap::GetSessionId(std::string group_id) {
  return GetInstance()->GetSessionIdInternal(group_id);
}

CastSessionIdMap::CastSessionIdMap(base::SequencedTaskRunner* task_runner)
    : supports_group_id_(
          base::FeatureList::IsEnabled(features::kAudioServiceAudioStreams)),
      task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(task_runner_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CastSessionIdMap::~CastSessionIdMap() = default;

void CastSessionIdMap::SetSessionIdInternal(
    std::string session_id,
    base::UnguessableToken group_id,
    content::WebContents* web_contents) {
  if (!supports_group_id_)
    return;

  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastSessionIdMap::SetSessionIdInternal,
                       weak_factory_.GetWeakPtr(), std::move(session_id),
                       group_id, web_contents));
    return;
  }

  // This check is required to bind to the current sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  DCHECK(GetSessionIdInternal(group_id.ToString()).empty());

  VLOG(1) << "Mapping session_id=" << session_id
          << " to group_id=" << group_id.ToString();
  // Unretained is safe because the GroupObserver is always owned by the
  // CastSessionIdMap.
  auto destroyed_callback = base::BindOnce(&CastSessionIdMap::OnGroupDestroyed,
                                           base::Unretained(this));
  auto group_observer = std::make_unique<GroupObserver>(
      web_contents, std::move(destroyed_callback));
  auto group_data = std::make_pair(session_id, std::move(group_observer));
  mapping_.emplace(group_id.ToString(), std::move(group_data));
}

std::string CastSessionIdMap::GetSessionIdInternal(std::string group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!supports_group_id_)
    return std::string();

  auto it = mapping_.find(group_id);
  if (it != mapping_.end())
    return it->second.first;
  return std::string();
}

void CastSessionIdMap::OnGroupDestroyed(base::UnguessableToken group_id) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CastSessionIdMap::RemoveGroupId,
                                        weak_factory_.GetWeakPtr(), group_id));
}

void CastSessionIdMap::RemoveGroupId(base::UnguessableToken group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = mapping_.find(group_id.ToString());
  if (it != mapping_.end()) {
    VLOG(1) << "Removing mapping for session_id=" << it->second.first
            << " to group_id=" << group_id.ToString();
    mapping_.erase(it);
  }
}

}  // namespace shell
}  // namespace chromecast
