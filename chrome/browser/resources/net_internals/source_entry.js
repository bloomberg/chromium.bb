// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SourceEntry = (function() {
  'use strict';

  /**
   * A SourceEntry gathers all log entries with the same source.
   *
   * @constructor
   */
  function SourceEntry(logEntry, maxPreviousSourceId) {
    this.maxPreviousSourceId_ = maxPreviousSourceId;
    this.entries_ = [];
    this.description_ = '';

    // Set to true on most net errors.
    this.isError_ = false;

    // If the first entry is a BEGIN_PHASE, set to false.
    // Set to true when an END_PHASE matching the first entry is encountered.
    this.isInactive_ = true;

    if (logEntry.phase == LogEventPhase.PHASE_BEGIN)
      this.isInactive_ = false;

    this.update(logEntry);
  }

  SourceEntry.prototype = {
    update: function(logEntry) {
      // Only the last event should have the same type first event,
      if (!this.isInactive_ &&
          logEntry.phase == LogEventPhase.PHASE_END &&
          logEntry.type == this.entries_[0].type) {
        this.isInactive_ = true;
      }

      // If we have a net error code, update |this.isError_| if apporpriate.
      if (logEntry.params) {
        var netErrorCode = logEntry.params.net_error;
        // Skip both cases where netErrorCode is undefined, and cases where it
        // is 0, indicating no actual error occurred.
        if (netErrorCode) {
          // Ignore error code caused by not finding an entry in the cache.
          if (logEntry.type != LogEventType.HTTP_CACHE_OPEN_ENTRY ||
              netErrorCode != NetError.FAILED) {
            this.isError_ = true;
          }
        }
      }

      var prevStartEntry = this.getStartEntry_();
      this.entries_.push(logEntry);
      var curStartEntry = this.getStartEntry_();

      // If we just got the first entry for this source.
      if (prevStartEntry != curStartEntry)
        this.updateDescription_();
    },

    updateDescription_: function() {
      var e = this.getStartEntry_();
      this.description_ = '';
      if (!e)
        return;

      if (e.source.type == LogSourceType.NONE) {
        // NONE is what we use for global events that aren't actually grouped
        // by a "source ID", so we will just stringize the event's type.
        this.description_ = getKeyWithValue(LogEventType, e.type);
        return;
      }

      if (e.params == undefined) {
        return;
      }

      switch (e.source.type) {
        case LogSourceType.URL_REQUEST:
        case LogSourceType.SOCKET_STREAM:
        case LogSourceType.HTTP_STREAM_JOB:
          this.description_ = e.params.url;
          break;
        case LogSourceType.CONNECT_JOB:
          this.description_ = e.params.group_name;
          break;
        case LogSourceType.HOST_RESOLVER_IMPL_REQUEST:
        case LogSourceType.HOST_RESOLVER_IMPL_JOB:
          this.description_ = e.params.host;
          break;
        case LogSourceType.DISK_CACHE_ENTRY:
        case LogSourceType.MEMORY_CACHE_ENTRY:
          this.description_ = e.params.key;
          break;
        case LogSourceType.SPDY_SESSION:
          if (e.params.host)
            this.description_ = e.params.host + ' (' + e.params.proxy + ')';
          break;
        case LogSourceType.SOCKET:
          // Use description of parent source, if any.
          if (e.params.source_dependency != undefined) {
            var parentId = e.params.source_dependency.id;
            this.description_ =
                g_browser.sourceTracker.getDescription(parentId);
          }
          break;
        case LogSourceType.UDP_SOCKET:
          if (e.params.address != undefined) {
            this.description_ = e.params.address;
            // If the parent of |this| is a DNS_TRANSACTION, use
            // '<DNS Server IP> [<DNS we're resolving>]'.
            if (this.entries_[0].type == LogEventType.SOCKET_ALIVE &&
                this.entries_[0].params.source_dependency != undefined) {
              var parentId = this.entries_[0].params.source_dependency.id;
              var parent = g_browser.sourceTracker.getSourceEntry(parentId);
              if (parent &&
                  parent.getSourceType() == LogSourceType.DNS_TRANSACTION &&
                  parent.getDescription().length > 0) {
                this.description_ += ' [' + parent.getDescription() + ']';
              }
            }
          }
          break;
        case LogSourceType.ASYNC_HOST_RESOLVER_REQUEST:
        case LogSourceType.DNS_TRANSACTION:
          this.description_ = e.params.hostname;
          break;
      }

      if (this.description_ == undefined)
        this.description_ = '';
    },

    /**
     * Returns a description for this source log stream, which will be displayed
     * in the list view. Most often this is a URL that identifies the request,
     * or a hostname for a connect job, etc...
     */
    getDescription: function() {
      return this.description_;
    },

    /**
     * Returns the starting entry for this source. Conceptually this is the
     * first entry that was logged to this source. However, we skip over the
     * TYPE_REQUEST_ALIVE entries which wrap TYPE_URL_REQUEST_START_JOB /
     * TYPE_SOCKET_STREAM_CONNECT.
     */
    getStartEntry_: function() {
      if (this.entries_.length < 1)
        return undefined;
      if (this.entries_.length >= 2) {
        if (this.entries_[0].type == LogEventType.REQUEST_ALIVE ||
            this.entries_[0].type == LogEventType.SOCKET_POOL_CONNECT_JOB ||
            this.entries_[1].type == LogEventType.UDP_CONNECT) {
          return this.entries_[1];
        }
      }
      return this.entries_[0];
    },

    getLogEntries: function() {
      return this.entries_;
    },

    getSourceTypeString: function() {
      return getKeyWithValue(LogSourceType, this.entries_[0].source.type);
    },

    getSourceType: function() {
      return this.entries_[0].source.type;
    },

    getSourceId: function() {
      return this.entries_[0].source.id;
    },

    /**
     * Returns the largest source ID seen before this object was received.
     * Used only for sorting SourceEntries without a source by source ID.
     */
    getMaxPreviousEntrySourceId: function() {
      return this.maxPreviousSourceId_;
    },

    isInactive: function() {
      return this.isInactive_;
    },

    isError: function() {
      return this.isError_;
    },

    /**
     * Returns time of last event if inactive.  Returns current time otherwise.
     */
    getEndTime: function() {
      if (!this.isInactive_) {
        return timeutil.getCurrentTime();
      } else {
        var endTicks = this.entries_[this.entries_.length - 1].time;
        return timeutil.convertTimeTicksToDate(endTicks).getTime();
      }
    },

    /**
     * Returns the time between the first and last events with a matching
     * source ID.  If source is still active, uses the current time for the
     * last event.
     */
    getDuration: function() {
      var startTicks = this.entries_[0].time;
      var startTime = timeutil.convertTimeTicksToDate(startTicks).getTime();
      var endTime = this.getEndTime();
      return endTime - startTime;
    },

    printAsText: function() {
      return PrintSourceEntriesAsText(this.entries_);
    }
  };

  return SourceEntry;
})();
