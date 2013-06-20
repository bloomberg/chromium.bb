// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Worker for requests. Fetches requests from a queue and processes them
 * synchronously, taking into account priorities. The highest priority is 0.
 */
function Worker() {
  /**
   * List of requests waiting to be checked. If these items are available in
   * cache, then they are processed immediately after starting the worker.
   * However, if they have to be downloaded, then these requests are moved
   * to pendingRequests_.
   *
   * @type {Request}
   * @private
   */
  this.newRequests_ = [];

  /**
   * List of pending requests for images to be downloaded.
   * @type {Request}
   * @private
   */
  this.pendingRequests_ = [];

  /**
   * List of requests being processed.
   * @type {Request}
   * @private
   */
  this.activeRequests_ = [];

  /**
   * If the worker has been started.
   * @type {boolean}
   * @private
   */
  this.started_ = false;
}

/**
 * Maximum download requests to be run in parallel.
 * @type {number}
 * @const
 */
Worker.MAXIMUM_IN_PARALLEL = 5;

/**
 * Adds a request to the internal priority queue and executes it when requests
 * with higher priorities are finished. If the result is cached, then it is
 * processed immediately once the worker is started.
 *
 * @param {Request} request Request object.
 */
Worker.prototype.add = function(request) {
  if (!this.started_) {
    this.newRequests_.push(request);
    return;
  }

  // Already started, so cache is available. Items available in cache will
  // be served immediately, other enqueued.
  this.serveCachedOrEnqueue_(request);
};

/**
 * Serves cached image or adds the request to the pending list.
 *
 * @param {Request} request Request object.
 * @private
 */
Worker.prototype.serveCachedOrEnqueue_ = function(request) {
  request.loadFromCacheAndProcess(function() {}, function() {
    // Not available in cache.
    this.pendingRequests_.push(request);

    // Sort requests by priorities.
    this.pendingRequests_.sort(function(a, b) {
      return a.getPriority() - b.getPriority();
    });

    // Continue handling the most important requests (if started).
    if (this.started_)
      this.continue_();
  }.bind(this));
};

/**
 * Starts handling requests.
 */
Worker.prototype.start = function() {
  this.started_ = true;

  // Process tasks added before worker has been started.
  for (var index = 0; index < this.newRequests_.length; index++) {
    this.serveCachedOrEnqueue_(this.newRequests_[index]);
  }
  this.newRequests_ = [];

  // Start serving enqueued requests.
  this.continue_();
};

/**
 * Processes pending requests from the queue. There is no guarantee that
 * all of the tasks will be processed at once.
 *
 * @private
 */
Worker.prototype.continue_ = function() {
  for (var index = 0; index < this.pendingRequests_.length; index++) {
    var request = this.pendingRequests_[index];

    // Run only up to MAXIMUM_IN_PARALLEL in the same time.
    if (Object.keys(this.activeRequests_).length ==
        Worker.MAXIMUM_IN_PARALLEL) {
      return;
    }

    delete this.pendingRequests_.splice(index, 1);
    this.activeRequests_.push(request);

    request.downloadAndProcess(this.finish_.bind(this, request));
  }
};

/**
 * Handles finished requests.
 *
 * @param {Request} request Finished request.
 * @private
 */
Worker.prototype.finish_ = function(request) {
  var index = this.activeRequests_.indexOf(request);
  if (index < 0)
    console.warn('Request not found.');
  delete this.activeRequests_.splice(index, 1);

  // Continue handling the most important requests (if started).
  if (this.started_)
    this.continue_();
};
