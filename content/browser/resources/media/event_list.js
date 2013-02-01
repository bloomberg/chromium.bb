// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {
  'use strict';

  /**
   * This class holds a list of MediaLogEvents.
   * It inherits from <li> and contains a tabular list of said events,
   * the time at which they occurred, and their parameters.
   */
  var EventList = cr.ui.define('li');

  EventList.prototype = {
    __proto__: HTMLLIElement.prototype,
    startTime_: null,

    /**
     * Decorate this list item as an EventList.
     */
    decorate: function() {
      this.table_ = document.createElement('table');
      var details = document.createElement('details');
      var summary = media.makeElement('summary', 'Log:');
      details.appendChild(summary);
      details.appendChild(this.table_);
      this.appendChild(details);

      var hRow = document.createElement('tr');
      hRow.appendChild(media.makeElement('th', 'Time:'));
      hRow.appendChild(media.makeElement('th', 'Event:'));
      hRow.appendChild(media.makeElement('th', 'Parameters:'));
      var header = document.createElement('thead');
      header.appendChild(hRow);
      this.table_.appendChild(header);
    },

    /**
     * Add an event to the list. It is stored as a new row in this.table_.
     * @param {Object} event The MediaLogEvent that has occurred.
     */
    addEvent: function(event) {
      var timeInMs = event.time * 1000;  // Work with milliseconds.
      this.startTime_ = this.startTime_ || timeInMs;
      timeInMs -= this.startTime_;

      var row = document.createElement('tr');
      row.appendChild(media.makeElement('td', timeInMs.toFixed(1)));
      row.appendChild(media.makeElement('td', event.type));
      var params = [];
      for (var key in event.params) {
        params.push(key + ': ' + event.params[key]);
      }

      row.appendChild(media.makeElement('td', params.join(', ')));
      this.table_.appendChild(row);
    }
  };

  return {
    EventList: EventList
  };
});
