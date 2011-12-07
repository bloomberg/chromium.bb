// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Anonymous namespace
(function() {

/**
 * Checks the display on the DNS tab against the information it should be
 * displaying.
 * @param {object} hostResolverInfo Results from a host resolver info query.
 */
function checkDisplay(hostResolverInfo) {
  var family = hostResolverInfo.default_address_family;
  expectEquals(getKeyWithValue(AddressFamily, family),
               $(DnsView.DEFAULT_FAMILY_SPAN_ID).innerText);
  expectEquals(family == AddressFamily.ADDRESS_FAMILY_IPV4,
               netInternalsTest.isDisplayed($(DnsView.IPV6_DISABLED_SPAN_ID)));

  expectEquals(hostResolverInfo.cache.capacity,
               parseInt($(DnsView.CAPACITY_SPAN_ID).innerText));
  expectEquals(hostResolverInfo.cache.ttl_success_ms,
               parseInt($(DnsView.TTL_SUCCESS_SPAN_ID).innerText));
  expectEquals(hostResolverInfo.cache.ttl_failure_ms,
               parseInt($(DnsView.TTL_FAILURE_SPAN_ID).innerText));

  var entries = hostResolverInfo.cache.entries;

  // Don't check exact displayed values, to avoid any races, but make sure
  // values are non-negative and have the correct sum.
  var active = parseInt($(DnsView.ACTIVE_SPAN_ID).innerText);
  var expired = parseInt($(DnsView.EXPIRED_SPAN_ID).innerText);
  expectLE(0, active);
  expectLE(0, expired);
  expectEquals(entries.length, active + expired);

  var tableId = DnsView.CACHE_TBODY_ID;
  netInternalsTest.checkStyledTableRows(tableId, entries.length);

  // Rather than check the exact string in every position, just make sure every
  // entry is not empty, and does not have 'undefined' anywhere, which should
  // find a fair number of potential output errors, without duplicating the
  // entire corresponding function of DnsView.
  for (var row = 0; row < entries.length; ++row) {
    for (column = 0; column < 4; ++column) {
      var text = netInternalsTest.getStyledTableText(tableId, row, column);
      expectNotEquals(text, '');
      expectFalse(/undefined/i.test(text));
    }
  }
}

/**
 * Finds an entry with the specified host name in the |hostResolverInfo| cache,
 * and returns its index.
 * @param {object} hostResolverInfo Results to search.
 * @param {object} hostname The host name to find.
 * @return {int} Index of the specified host.  -1 if not found.
 */
function findEntry(hostResolverInfo, hostname) {
  var entries = hostResolverInfo.cache.entries;
  for (var i = 0; i < entries.length; ++i) {
    if (entries[i].hostname == hostname)
      return i;
  }
  return -1;
}

/**
 * A Task that adds a hostname to the cache and waits for it to appear in the
 * data we receive from the cache.
 * @param {string} hostname Name of host address we're waiting for.
 * @param {string} ipAddress IP address we expect it to have.  Null if we expect
 *     a net error other than OK.
 * @param {int} netError The expected network error code.
 * @param {bool} expired True if we expect the entry to be expired.  The added
 *     entry will have an expiration time far enough away from the current time
 *     that there will be no chance of any races.
 * @extends {netInternalsTest.Task}
 */
function AddCacheEntryTask(hostname, ipAddress, netError, expired) {
  this.hostname_ = hostname;
  this.ipAddress_ = ipAddress;
  this.netError_ = netError;
  this.expired_ = expired;
  netInternalsTest.Task.call(this);
}

AddCacheEntryTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  /**
   * Adds an entry to the cache and starts waiting to received the results from
   * the browser process.
   */
  start: function() {
    var addCacheEntryParams = [
      this.hostname_,
      this.ipAddress_,
      this.netError_,
      this.expired_ ? -2 : 2
    ];
    chrome.send('addCacheEntry', addCacheEntryParams);
    g_browser.addHostResolverInfoObserver(this, false);
  },

  /**
   * Callback from the BrowserBridge.  Checks if |hostResolverInfo| has the
   * DNS entry specified on creation.  If so, validates it and completes the
   * task.  If not, continues running.
   * @param {object} hostResolverInfo Results a host resolver info query.
   */
  onHostResolverInfoChanged: function(hostResolverInfo) {
    if (!this.isDone()) {
      checkDisplay(hostResolverInfo);

      var index = findEntry(hostResolverInfo, this.hostname_);
      if (index >= 0) {
        var entry = hostResolverInfo.cache.entries[index];
        if (this.netError_) {
          this.checkError_(entry);
        } else {
          this.checkSuccess_(entry);
        }
        var expirationDate = timeutil.convertTimeTicksToDate(entry.expiration);
        expectEquals(this.expired_, expirationDate < new Date());

        // Expect at least one active or expired entry, depending on |expired_|.
        // To avoid any chance of a race, exact values are not tested.
        var activeMin = this.expired_ ? 0 : 1;
        var expiredMin = this.expired_ ? 1 : 0;
        expectLE(activeMin, parseInt($(DnsView.ACTIVE_SPAN_ID).innerText));
        expectLE(expiredMin, parseInt($(DnsView.EXPIRED_SPAN_ID).innerText));

        // Text for the expiration time of the entry should contain 'Expired'
        // only if |expired_| is true.  Only checked for entries we add
        // ourselves to avoid any expiration time race.
        var expirationText =
            netInternalsTest.getStyledTableText(DnsView.CACHE_TBODY_ID,
                                                index, 3);
        expectEquals(this.expired_, /expired/i.test(expirationText));

        this.onTaskDone();
      }
    }
  },

  checkError_: function(entry) {
    expectEquals(this.netError_, entry.error);
  },

  checkSuccess_: function(entry) {
    expectEquals(undefined, entry.error);
    expectEquals(1, entry.addresses.length);
    expectEquals(0, entry.addresses[0].search(this.ipAddress_));
  }
};

/**
 * A Task clears the cache by simulating a button click.
 * @extends {netInternalsTest.Task}
 */
function ClearCacheTask() {
  netInternalsTest.Task.call(this);
}

ClearCacheTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  start: function() {
    $(DnsView.CLEAR_CACHE_BUTTON_ID).onclick();
    this.onTaskDone();
  }
};

/**
 * A Task that waits for the specified hostname entry to disappear from the
 * cache.
 * @param {string} hostname Name of host we're waiting to be removed.
 * @extends {netInternalsTest.Task}
 */
function WaitForEntryDestructionTask(hostname) {
  this.hostname_ = hostname;
  netInternalsTest.Task.call(this);
}

WaitForEntryDestructionTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  /**
   * Starts waiting to received the results from the browser process.
   */
  start: function() {
    g_browser.addHostResolverInfoObserver(this, false);
  },

  /**
   * Callback from the BrowserBridge.  Checks if the entry has been removed.
   * If so, the task completes.
   * @param {object} hostResolverInfo Results a host resolver info query.
   */
  onHostResolverInfoChanged: function(hostResolverInfo) {
    if (!this.isDone()) {
      checkDisplay(hostResolverInfo);

      var entry = findEntry(hostResolverInfo, this.hostname_);
      if (entry == -1)
        this.onTaskDone();
    }
  }
};

netInternalsTest.test('netInternalsDnsViewSuccess', function() {
  netInternalsTest.switchToView('dns');
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddCacheEntryTask(
                        'somewhere.com', '1.2.3.4', 0, false));
  taskQueue.addTask(new ClearCacheTask());
  taskQueue.addTask(new WaitForEntryDestructionTask('somewhere.com'));
  taskQueue.run(true);
});

netInternalsTest.test('netInternalsDnsViewFail', function() {
  netInternalsTest.switchToView('dns');
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddCacheEntryTask(
                        'nowhere.com', '', NetError.NAME_NOT_RESOLVED, false));
  taskQueue.addTask(new ClearCacheTask());
  taskQueue.addTask(new WaitForEntryDestructionTask('nowhere.com'));
  taskQueue.run(true);
});

netInternalsTest.test('netInternalsDnsViewExpired', function() {
  netInternalsTest.switchToView('dns');
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new AddCacheEntryTask(
                        'somewhere.com', '1.2.3.4', 0, true));
  taskQueue.addTask(new ClearCacheTask());
  taskQueue.addTask(new WaitForEntryDestructionTask('somewhere.com'));
  taskQueue.run(true);
});

netInternalsTest.test('netInternalsDnsViewAddTwoTwice', function() {
  netInternalsTest.switchToView('dns');
  var taskQueue = new netInternalsTest.TaskQueue(true);
  for (var i = 0; i < 2; ++i) {
    taskQueue.addTask(new AddCacheEntryTask(
                          'somewhere.com', '1.2.3.4', 0, false));
    taskQueue.addTask(new AddCacheEntryTask(
                          'nowhere.com', '', NetError.NAME_NOT_RESOLVED, true));
    taskQueue.addTask(new ClearCacheTask());
    taskQueue.addTask(new WaitForEntryDestructionTask('somewhere.com'));
    taskQueue.addTask(new WaitForEntryDestructionTask('nowhere.com'));
  }
  taskQueue.run(true);
});

netInternalsTest.test('netInternalsDnsViewIncognitoClears', function() {
  netInternalsTest.switchToView('dns');
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(netInternalsTest.getCreateIncognitoBrowserTask());
  taskQueue.addTask(new AddCacheEntryTask(
                        'somewhere.com', '1.2.3.4', 0, true));
  taskQueue.addTask(netInternalsTest.getCloseIncognitoBrowserTask());
  taskQueue.addTask(new WaitForEntryDestructionTask('somewhere.com'));
  taskQueue.run(true);
});

})();  // Anonymous namespace
