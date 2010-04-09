// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_util.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"

namespace net {
namespace {

class FormatHelper {
 public:
  std::string ToString(const std::vector<CapturingNetLog::Entry>& entries,
                       size_t num_entries_truncated) {
    entries_.clear();

    // Pass 1: Match the start/end of indentation blocks. Fills |entries_|
    // with the results.
    PopulateEntries(entries);

    // Pass 2: Figure out the maximum width of each column. This allows us
    // to right-justify text within each column.
    size_t max_time_width, max_indentation, max_type_width, max_dt_width;
    GetMaxExtent(
        &max_time_width, &max_indentation, &max_type_width, &max_dt_width);

    // Pass 3: Assemble the string.
    std::string result;

    const int kSpacesPerIndentation = 2;

    for (size_t i = 0; i < entries_.size(); ++i) {
      if (num_entries_truncated > 0 && i + 1 == entries_.size()) {
        StringAppendF(&result, " ... Truncated %" PRIuS " entries ...\n",
                      num_entries_truncated);
      }

      if (entries_[i].block_index != -1 &&
          static_cast<size_t>(entries_[i].block_index + 1) == i) {
        // If there were no entries in between the START/END block then don't
        // bother printing a line for END (it just adds noise, and we already
        // show the time delta besides START anyway).
        continue;
      }

      int indentation_spaces = entries_[i].indentation * kSpacesPerIndentation;
      std::string entry_str = GetEntryString(i);

      // Hack to better print known event types.
      if (entries_[i].log_entry->type == NetLog::TYPE_TODO_STRING ||
          entries_[i].log_entry->type == NetLog::TYPE_TODO_STRING_LITERAL) {
        // Don't display the TODO_STRING type.
        entry_str = StringPrintf(
            " \"%s\"",
            entries_[i].log_entry->extra_parameters->ToString().c_str());
      }

      StringAppendF(&result, "t=%s: %s%s",
                    PadStringLeft(GetTimeString(i), max_time_width).c_str(),
                    PadStringLeft("", indentation_spaces).c_str(),
                    entry_str.c_str());

      if (entries_[i].IsBeginEvent()) {
        // Summarize how long this block lasted.
        int padding = ((max_indentation - entries_[i].indentation) *
            kSpacesPerIndentation) + (max_type_width - entry_str.size());
        StringAppendF(&result, "%s [dt=%s]",
                      PadStringLeft("", padding).c_str(),
                      PadStringLeft(GetBlockDtString(i), max_dt_width).c_str());
      }

      // Append any custom parameters.
      NetLog::EventParameters* extra_params =
          entries_[i].log_entry->extra_parameters;
      NetLog::EventType type = entries_[i].log_entry->type;
      NetLog::EventPhase phase = entries_[i].log_entry->phase;

      if (type != NetLog::TYPE_TODO_STRING &&
          type != NetLog::TYPE_TODO_STRING_LITERAL &&
          extra_params) {
        std::string extra_details;

        // Hacks to better print known event types.
        if (type == NetLog::TYPE_URL_REQUEST_START ||
            type == NetLog::TYPE_SOCKET_STREAM_CONNECT) {
          if (phase == NetLog::PHASE_BEGIN) {
            extra_details =
                StringPrintf("url: %s", extra_params->ToString().c_str());
          } else if (phase == NetLog::PHASE_END) {
            int error_code = static_cast<NetLogIntegerParameter*>(
                extra_params)->value();
            extra_details = StringPrintf("net error: %d (%s)",
                                         error_code,
                                         ErrorToString(error_code));
          }
        } else if (type == NetLog::TYPE_SOCKET_POOL_CONNECT_JOB) {
          extra_details =
              StringPrintf("group: %s", extra_params->ToString().c_str());
        } else {
          extra_details = extra_params->ToString();
        }

        int indentation = max_time_width + indentation_spaces +
                          kSpacesPerIndentation + 5;

        StringAppendF(
            &result,
            "\n%s%s",
            PadStringLeft("", indentation).c_str(),
            extra_details.c_str());
      }

      if (i + 1 != entries_.size())
        result += "\n";
    }

    return result;
  }

 private:
  struct Entry {
    explicit Entry(const CapturingNetLog::Entry* log_entry)
        : log_entry(log_entry), indentation(0), block_index(-1) {}

    bool IsBeginEvent() const {
      return log_entry->phase == NetLog::PHASE_BEGIN;
    }

    bool IsEndEvent() const {
      return log_entry->phase == NetLog::PHASE_END;
    }

    const CapturingNetLog::Entry* log_entry;
    size_t indentation;
    int block_index;  // The index of the matching start / end of block.
  };

  void PopulateEntries(const std::vector<CapturingNetLog::Entry>& entries) {
    int current_indentation = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
      Entry entry(&entries[i]);

      entry.indentation = current_indentation;

      if (entry.IsBeginEvent()) {
        // Indent everything contained in this block.
        current_indentation++;
      }

      if (entry.IsEndEvent()) {
        int start_index = FindStartOfBlockIndex(entry);
        if (start_index != -1) {
          // Point the start / end of block at each other.
          entry.block_index = start_index;
          entries_[start_index].block_index = i;

          // Restore the indentation prior to the block.
          // (Could be more than 1 level if close of blocks are missing).
          current_indentation = entries_[start_index].indentation;
          entry.indentation = current_indentation;
        }
      }

      entries_.push_back(entry);
    }
  }

  int FindStartOfBlockIndex(const Entry& entry) {
    DCHECK(entry.IsEndEvent());

    // Find the matching  start of block by scanning backwards.
    for (int i = entries_.size() - 1; i >= 0; --i) {
      if (entries_[i].IsBeginEvent() &&
          entries_[i].log_entry->type == entry.log_entry->type) {
        return i;
      }
    }
    return -1;  // Start not found.
  }

  void GetMaxExtent(size_t* max_time_width,
                    size_t* max_indentation,
                    size_t* max_type_width,
                    size_t* max_dt_width) {
    *max_time_width = *max_indentation = *max_type_width = *max_dt_width = 0;
    for (size_t i = 0; i < entries_.size(); ++i) {
      *max_time_width = std::max(*max_time_width, GetTimeString(i).size());
      if (entries_[i].log_entry->phase != NetLog::PHASE_NONE)
        *max_type_width = std::max(*max_type_width, GetEntryString(i).size());
      *max_indentation = std::max(*max_indentation, entries_[i].indentation);

      if (entries_[i].IsBeginEvent())
        *max_dt_width = std::max(*max_dt_width, GetBlockDtString(i).size());
    }
  }

  std::string GetBlockDtString(size_t start_index) {
    int end_index = entries_[start_index].block_index;
    if (end_index == -1) {
      // Block is not closed, implicitly close it at EOF.
      end_index = entries_.size() - 1;
    }
    int64 dt_ms = (entries_[end_index].log_entry->time -
                   entries_[start_index].log_entry->time).InMilliseconds();

    return Int64ToString(dt_ms);
  }

  std::string GetTimeString(size_t index) {
    int64 t_ms = (entries_[index].log_entry->time -
                  base::TimeTicks()).InMilliseconds();
    return Int64ToString(t_ms);
  }

  std::string GetEntryString(size_t index) {
    const CapturingNetLog::Entry* entry = entries_[index].log_entry;

    std::string entry_str;
    NetLog::EventPhase phase = entry->phase;

    entry_str = NetLog::EventTypeToString(entry->type);

    if (phase == NetLog::PHASE_BEGIN &&
        index + 1 < entries_.size() &&
        static_cast<size_t>(entries_[index + 1].block_index) == index) {
      // If this starts an empty block, we will pretend it is a PHASE_NONE
      // so we don't print the "+" prefix.
      phase = NetLog::PHASE_NONE;
    }

    switch (phase) {
      case NetLog::PHASE_BEGIN:
        return std::string("+") + entry_str;
      case NetLog::PHASE_END:
        return std::string("-") + entry_str;
      case NetLog::PHASE_NONE:
        return std::string(" ") + entry_str;
      default:
        NOTREACHED();
        return std::string();
    }
  }

  static std::string PadStringLeft(const std::string& str, size_t width) {
    DCHECK_LE(str.size(), width);
    std::string padding;
    padding.resize(width - str.size(), ' ');
    return padding + str;
  }

  std::vector<Entry> entries_;
};

}  // namespace

// static
std::string NetLogUtil::PrettyPrintAsEventTree(
    const std::vector<CapturingNetLog::Entry>& entries,
    size_t num_entries_truncated) {
  FormatHelper helper;
  return helper.ToString(entries, num_entries_truncated);
}

}  // namespace net
