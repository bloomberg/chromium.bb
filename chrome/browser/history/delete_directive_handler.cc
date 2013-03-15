// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/delete_directive_handler.h"

#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "sync/api/sync_change.h"
#include "sync/protocol/history_delete_directive_specifics.pb.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"

namespace {

std::string DeleteDirectiveToString(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  scoped_ptr<base::DictionaryValue> value(
      syncer::HistoryDeleteDirectiveSpecificsToValue(delete_directive));
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

// Compare time range directives first by start time, then by end time.
bool TimeRangeLessThan(const syncer::SyncData& data1,
                       const syncer::SyncData& data2) {
  const sync_pb::TimeRangeDirective& range1 =
      data1.GetSpecifics().history_delete_directive().time_range_directive();
  const sync_pb::TimeRangeDirective& range2 =
      data2.GetSpecifics().history_delete_directive().time_range_directive();
  if (range1.start_time_usec() < range2.start_time_usec())
    return true;
  if (range1.start_time_usec() > range2.start_time_usec())
    return false;
  return range1.end_time_usec() < range2.end_time_usec();
}

// Converts a Unix timestamp in microseconds to a base::Time value.
base::Time UnixUsecToTime(int64 usec) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(usec);
}

// Converts a base::Time value to a Unix timestamp in microseconds.
int64 TimeToUnixUsec(base::Time time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

// Converts global IDs in |global_id_directive| to times.
void GetTimesFromGlobalIds(
    const sync_pb::GlobalIdDirective& global_id_directive,
    std::set<base::Time> *times) {
  for (int i = 0; i < global_id_directive.global_id_size(); ++i) {
    times->insert(
        base::Time::FromInternalValue(global_id_directive.global_id(i)));
  }
}

}  // anonymous namespace

namespace history {

class DeleteDirectiveHandler::DeleteDirectiveTask : public HistoryDBTask {
 public:
  DeleteDirectiveTask(
      base::WeakPtr<DeleteDirectiveHandler> delete_directive_handler,
      const syncer::SyncDataList& delete_directive,
      DeleteDirectiveHandler::PostProcessingAction post_processing_action)
     : delete_directive_handler_(delete_directive_handler),
       delete_directives_(delete_directive),
       post_processing_action_(post_processing_action) {}

  // Implements HistoryDBTask.
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE;
  virtual void DoneRunOnMainThread() OVERRIDE;

 private:
  virtual ~DeleteDirectiveTask() {}

  // Process a list of global Id directives. Delete all visits to a URL in
  // time ranges of directives if the timestamp of one visit matches with one
  // global id.
  void ProcessGlobalIdDeleteDirectives(
      history::HistoryBackend* history_backend,
      const syncer::SyncDataList& global_id_directives);

  // Process a list of time range directives, all history entries within the
  // time ranges are deleted. |time_range_directives| should be sorted by
  // |start_time_usec| and |end_time_usec| already.
  void ProcessTimeRangeDeleteDirectives(
      history::HistoryBackend* history_backend,
      const syncer::SyncDataList& time_range_directives);

  base::WeakPtr<DeleteDirectiveHandler> delete_directive_handler_;
  syncer::SyncDataList delete_directives_;
  DeleteDirectiveHandler::PostProcessingAction post_processing_action_;
};

bool DeleteDirectiveHandler::DeleteDirectiveTask::RunOnDBThread(
    history::HistoryBackend* backend,
    history::HistoryDatabase* db) {
  syncer::SyncDataList global_id_directives;
  syncer::SyncDataList time_range_directives;
  for (syncer::SyncDataList::const_iterator it = delete_directives_.begin();
       it != delete_directives_.end(); ++it) {
    DCHECK_EQ(it->GetDataType(), syncer::HISTORY_DELETE_DIRECTIVES);
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive =
        it->GetSpecifics().history_delete_directive();
    if (delete_directive.has_global_id_directive()) {
      global_id_directives.push_back(*it);
    } else {
      time_range_directives.push_back(*it);
    }
  }

  ProcessGlobalIdDeleteDirectives(backend, global_id_directives);
  std::sort(time_range_directives.begin(), time_range_directives.end(),
            TimeRangeLessThan);
  ProcessTimeRangeDeleteDirectives(backend, time_range_directives);
  return true;
}

void DeleteDirectiveHandler::DeleteDirectiveTask::DoneRunOnMainThread() {
  if (delete_directive_handler_.get()) {
    delete_directive_handler_->FinishProcessing(post_processing_action_,
                                                delete_directives_);
  }
}

void
DeleteDirectiveHandler::DeleteDirectiveTask::ProcessGlobalIdDeleteDirectives(
    history::HistoryBackend* history_backend,
    const syncer::SyncDataList& global_id_directives) {
  if (global_id_directives.empty())
    return;

  // Group times represented by global IDs by time ranges of delete directives.
  // It's more efficient for backend to process all directives with same time
  // range at once.
  typedef std::map<std::pair<base::Time, base::Time>, std::set<base::Time> >
      GlobalIdTimesGroup;
  GlobalIdTimesGroup id_times_group;
  for (size_t i = 0; i < global_id_directives.size(); ++i) {
    DVLOG(1) << "Processing delete directive: "
             << DeleteDirectiveToString(
                    global_id_directives[i].GetSpecifics()
                        .history_delete_directive());

    const sync_pb::GlobalIdDirective& id_directive =
        global_id_directives[i].GetSpecifics().history_delete_directive()
            .global_id_directive();
    if (id_directive.global_id_size() == 0 ||
        !id_directive.has_start_time_usec() ||
        !id_directive.has_end_time_usec()) {
      DLOG(ERROR) << "Invalid global id directive.";
      continue;
    }
    GetTimesFromGlobalIds(
        id_directive,
        &id_times_group[
            std::make_pair(UnixUsecToTime(id_directive.start_time_usec()),
                           UnixUsecToTime(id_directive.end_time_usec()))]);
  }

  if (id_times_group.empty())
    return;

  // Call backend to expire history of directives in each group.
  for (GlobalIdTimesGroup::const_iterator group_it = id_times_group.begin();
      group_it != id_times_group.end(); ++group_it) {
    // Add 1us to cover history entries visited at the end time because time
    // range in directive is inclusive.
    history_backend->ExpireHistoryForTimes(
        group_it->second,
        group_it->first.first,
        group_it->first.second + base::TimeDelta::FromMicroseconds(1));
  }
}

void
DeleteDirectiveHandler::DeleteDirectiveTask::ProcessTimeRangeDeleteDirectives(
    history::HistoryBackend* history_backend,
    const syncer::SyncDataList& time_range_directives) {
  if (time_range_directives.empty())
    return;

  // Iterate through time range directives. Expire history in combined
  // time range for multiple directives whose time ranges overlap.
  base::Time current_start_time;
  base::Time current_end_time;
  for (size_t i = 0; i < time_range_directives.size(); ++i) {
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive =
        time_range_directives[i].GetSpecifics().history_delete_directive();
    DVLOG(1) << "Processing time range directive: "
             << DeleteDirectiveToString(delete_directive);

    const sync_pb::TimeRangeDirective& time_range_directive =
        delete_directive.time_range_directive();
    if (!time_range_directive.has_start_time_usec() ||
        !time_range_directive.has_end_time_usec() ||
        time_range_directive.start_time_usec() >=
            time_range_directive.end_time_usec()) {
      DLOG(ERROR) << "Invalid time range directive.";
      continue;
    }

    base::Time directive_start_time =
        UnixUsecToTime(time_range_directive.start_time_usec());
    base::Time directive_end_time =
        UnixUsecToTime(time_range_directive.end_time_usec());
    if (directive_start_time > current_end_time) {
      if (!current_start_time.is_null()) {
        // Add 1us to cover history entries visited at the end time because
        // time range in directive is inclusive.
        history_backend->ExpireHistoryBetween(
            std::set<GURL>(), current_start_time,
            current_end_time + base::TimeDelta::FromMicroseconds(1));
      }
      current_start_time = directive_start_time;
    }
    if (directive_end_time > current_end_time)
      current_end_time = directive_end_time;
  }

  if (!current_start_time.is_null()) {
    history_backend->ExpireHistoryBetween(
        std::set<GURL>(), current_start_time,
        current_end_time + base::TimeDelta::FromMicroseconds(1));
  }
}

DeleteDirectiveHandler::DeleteDirectiveHandler()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

DeleteDirectiveHandler::~DeleteDirectiveHandler() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DeleteDirectiveHandler::Start(
    HistoryService* history_service,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_processor_ = sync_processor.Pass();
  if (!initial_sync_data.empty()) {
    // Drop processed delete directives during startup.
    history_service->ScheduleDBTask(
        new DeleteDirectiveTask(weak_ptr_factory_.GetWeakPtr(),
                                initial_sync_data,
                                DROP_AFTER_PROCESSING),
        &internal_consumer_);
  }
}

void DeleteDirectiveHandler::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sync_processor_.reset();
}

bool DeleteDirectiveHandler::CreateDeleteDirectives(
    const std::set<int64>& global_ids,
    base::Time begin_time,
    base::Time end_time) {
  base::Time now = base::Time::Now();
  sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;

  int64 begin_time_usecs = TimeToUnixUsec(begin_time);
  // Delete directives require a non-null begin time.
  if (begin_time.is_null())
    begin_time_usecs += 1;

  // Determine the actual end time -- it should not be null or in the future.
  // TODO(dubroy): Use sane time (crbug.com/146090) here when it's available.
  base::Time end = (end_time.is_null() || end_time > now) ? now : end_time;
  // -1 because end time in delete directives is inclusive.
  int64 end_time_usecs = TimeToUnixUsec(end) - 1;

  if (global_ids.empty()) {
    sync_pb::TimeRangeDirective* time_range_directive =
        delete_directive.mutable_time_range_directive();
    time_range_directive->set_start_time_usec(begin_time_usecs);
    time_range_directive->set_end_time_usec(end_time_usecs);
  } else {
    for (std::set<int64>::const_iterator it = global_ids.begin();
         it != global_ids.end(); ++it) {
      sync_pb::GlobalIdDirective* global_id_directive =
          delete_directive.mutable_global_id_directive();
      global_id_directive->add_global_id(*it);
      global_id_directive->set_start_time_usec(begin_time_usecs);
      global_id_directive->set_end_time_usec(end_time_usecs);
    }
  }
  syncer::SyncError error = ProcessLocalDeleteDirective(delete_directive);
  return !error.IsSet();
}

syncer::SyncError DeleteDirectiveHandler::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sync_processor_.get()) {
    return syncer::SyncError(
        FROM_HERE,
        "Cannot send local delete directive to sync",
        syncer::HISTORY_DELETE_DIRECTIVES);
  }
  // Generate a random sync tag since history delete directives don't
  // have a 'built-in' ID.  8 bytes should suffice.
  std::string sync_tag = base::RandBytesAsString(8);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_history_delete_directive()->CopyFrom(
      delete_directive);
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(
          sync_tag, sync_tag, entity_specifics);
  syncer::SyncChange change(
      FROM_HERE, syncer::SyncChange::ACTION_ADD, sync_data);
  syncer::SyncChangeList changes(1, change);
  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

syncer::SyncError DeleteDirectiveHandler::ProcessSyncChanges(
    HistoryService* history_service,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!sync_processor_.get()) {
    return syncer::SyncError(
        FROM_HERE, "Sync is disabled.", syncer::HISTORY_DELETE_DIRECTIVES);
  }

  syncer::SyncDataList delete_directives;
  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        delete_directives.push_back(it->sync_data());
        break;
      case syncer::SyncChange::ACTION_DELETE:
        // TODO(akalin): Keep track of existing delete directives.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  if (!delete_directives.empty()) {
    // Don't drop real-time delete directive so that sync engine can detect
    // redelivered delete directives to avoid processing them again and again
    // in one chrome session.
    history_service->ScheduleDBTask(
        new DeleteDirectiveTask(weak_ptr_factory_.GetWeakPtr(),
                                delete_directives, KEEP_AFTER_PROCESSING),
        &internal_consumer_);
  }
  return syncer::SyncError();
}

void DeleteDirectiveHandler::FinishProcessing(
    PostProcessingAction post_processing_action,
    const syncer::SyncDataList& delete_directives) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If specified, drop processed delete directive in sync model because they
  // only need to be applied once.
  if (sync_processor_.get() &&
      post_processing_action == DROP_AFTER_PROCESSING) {
    syncer::SyncChangeList change_list;
    for (size_t i = 0; i < delete_directives.size(); ++i) {
      change_list.push_back(
          syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                             delete_directives[i]));
    }
    sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  }
}

}  // namespace history
