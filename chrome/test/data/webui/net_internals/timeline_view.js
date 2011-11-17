// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Anonymous namespace
(function() {

// Range of time set on a log once loaded used by sanity checks.
// Used by sanityCheckWithTimeRange.
var startTime = null;
var endTime = null;

var timelineView = TimelineView.getInstance();
var graphView = timelineView.graphView_;
var scrollbar = graphView.scrollbar_;
var canvas = graphView.canvas_;

/**
 * A Task that creates a log dump, modifies it so |timeTicks| are all in UTC,
 * clears all events from the log, and then adds two new SOCKET events, which
 * have the specified start and end times.
 *
 * Most of these tests start with this task first.  This gives us a known
 * starting state, and prevents the data from automatically updating.
 *
 * @param {int} startTime Time of the begin event.
 * @param {int} endTime Time of the end event.
 * @extends {netInternalsTest.Task}
 */
function LoadLogWithNewEventsTask(startTime, endTime) {
  netInternalsTest.Task.call(this);
  this.startTime_ = startTime;
  this.endTime_ = endTime;
}

LoadLogWithNewEventsTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  /**
   * Starts creating a log dump.
   */
  start: function() {
    logutil.createLogDumpAsync('test', this.onLogDumpCreated.bind(this));
  },

  /**
   * Modifies the log dump and loads it.
   */
  onLogDumpCreated: function(logDumpText) {
    var logDump = JSON.parse(logDumpText);

    logDump.constants.timeTickOffset = '0';
    logDump.events = [];

    var source = new netInternalsTest.Source(1, LogSourceType.SOCKET);
    logDump.events.push(
        netInternalsTest.CreateBeginEvent(source, LogEventType.SOCKET_ALIVE,
                                          this.startTime_, null));
    logDump.events.push(
        netInternalsTest.CreateMatchingEndEvent(logDump.events[0],
                                                this.endTime_, null));
    logDumpText = JSON.stringify(logDump);

    assertEquals('Log loaded.', logutil.loadLogFile(logDumpText));

    endTime = this.endTime_;
    startTime = this.startTime_;
    if (startTime >= endTime)
      --startTime;

    sanityCheckWithTimeRange(false);

    this.onTaskDone();
  }
};

/**
 * Checks certain invariant properties of the TimelineGraphView and the
 * scroll bar.
 */
function sanityCheck() {
  expectLT(graphView.startTime_, graphView.endTime_);
  expectLE(0, scrollbar.getPosition());
  expectLE(scrollbar.getPosition(), scrollbar.getRange());
}

/**
 * Checks what sanityCheck does, but also checks that |startTime| and |endTime|
 * are the same as those used by the graph, as well as whether we have a timer
 * running to update the graph's end time.  To avoid flake, this should only
 * be used synchronously relative to when |startTime| and |endTime| were set,
 * unless |expectUpdateTimer| is false.
 * @param {bool} expectUpdateTimer true if the TimelineView should currently
 *     have an update end time timer running.
 */
function sanityCheckWithTimeRange(expectUpdateTimer) {
  if (!expectUpdateTimer) {
    expectEquals(null, timelineView.updateIntervalId_);
  } else {
    expectNotEquals(null, timelineView.updateIntervalId_);
  }
  assertNotEquals(startTime, null);
  assertNotEquals(endTime, null);
  expectEquals(startTime, graphView.startTime_);
  expectEquals(endTime, graphView.endTime_);
  sanityCheck(false);
}

/**
 * Checks what sanityCheck does, but also checks that |startTime| and |endTime|
 * are the same as those used by the graph.
 */
function sanityCheckNotUpdating() {
  expectEquals(null, timelineView.updateIntervalId_);
  sanityCheckWithTimeRange();
}

/**
 * Simulates mouse wheel movement over the canvas element.
 * @param {number} mouseWheelMovement Amount of movement to simulate.
 */
function mouseZoom(mouseWheelMovement) {
  var scrollbarStartedAtEnd =
      (scrollbar.getRange() == scrollbar.getPosition());

  var event = document.createEvent('WheelEvent');
  event.initWebKitWheelEvent(0, mouseWheelMovement, window, 0, 0, 0, 0,
                             false, false, false, false);
  canvas.dispatchEvent(event);

  // If the scrollbar started at the end of the range, make sure it ends there
  // as well.
  if (scrollbarStartedAtEnd)
    expectEquals(scrollbar.getRange(), scrollbar.getPosition());

  sanityCheck();
}

/**
 * Simulates moving the mouse wheel up.
 */
function mouseZoomIn() {
  var oldScale = graphView.scale_;
  var oldRange = scrollbar.getRange();

  mouseZoom(1);

  if (oldScale == graphView.scale_) {
    expectEquals(oldScale, TimelineGraphView.MIN_SCALE);
  } else {
    expectLT(graphView.scale_, oldScale);
  }
  expectGE(scrollbar.getRange(), oldRange);
}

/**
 * Simulates moving the mouse wheel down.
 */
function mouseZoomOut() {
  var oldScale = graphView.scale_;
  var oldRange = scrollbar.getRange();

  mouseZoom(-1);

  expectGT(graphView.scale_, oldScale);
  expectLE(scrollbar.getRange(), oldRange);
}

/**
 * Simulates zooming all the way by multiple mouse wheel events.
 */
function mouseZoomAllTheWayIn() {
  expectLT(TimelineGraphView.MIN_SCALE, graphView.scale_);
  while (graphView.scale_ != TimelineGraphView.MIN_SCALE)
    mouseZoomIn();
  // Verify that zooming in when already at max zoom works.
  mouseZoomIn();
}

/**
 * A Task that scrolls the scrollbar by manipulating the DOM, and then waits
 * for the scroll to complete.  Has to be a task because onscroll and DOM
 * manipulations both occur asynchronously.
 *
 * Not safe to use when other asynchronously running code may try to
 * manipulate the scrollbar itself, or adjust the length of the scrollbar.
 *
 * @param {int} position Position to scroll to.
 * @extends {netInternalsTest.Task}
 */
function MouseScrollTask(position) {
  netInternalsTest.Task.call(this);
  this.position_ = position;
  // If the scrollbar's |position| and its node's |scrollLeft| values don't
  // currently match, we set this to true and wait for |scrollLeft| to be
  // updated, which will trigger an onscroll event.
  this.waitingToStart_ = false;
}

MouseScrollTask.prototype = {
  __proto__: netInternalsTest.Task.prototype,

  start: function() {
    this.waitingToStart_ = false;
    // If the scrollbar is already in the correct position, do nothing.
    if (scrollbar.getNode().scrollLeft == this.position_) {
      // We may still have a timer going to adjust the position of the
      // scrollbar to some other value.  If so, this will clear it.
      scrollbar.setPosition(this.position_);
      this.onTaskDone();
      return;
    }

    // Replace the onscroll event handler with our own.
    this.oldOnScroll_ = scrollbar.getNode().onscroll;
    scrollbar.getNode().onscroll = this.onScroll_.bind(this);
    if (scrollbar.getNode().scrollLeft != scrollbar.getPosition()) {
      this.waitingToStart_ = true;
      return;
    }

    window.setTimeout(this.startScrolling_.bind(this), 0);
  },

  onScroll_: function(event) {
    // Restore the original onscroll function.
    scrollbar.getNode().onscroll = this.oldOnScroll_;
    // Call the original onscroll function.
    this.oldOnScroll_(event);

    if (this.waitingToStart_) {
      this.start();
      return;
    }

    assertEquals(this.position_, scrollbar.getNode().scrollLeft);
    assertEquals(this.position_, scrollbar.getPosition());

    sanityCheck();
    this.onTaskDone();
  },

  startScrolling_: function() {
    scrollbar.getNode().scrollLeft = this.position_;
  }
};

netInternalsTest.test('netInternalsTimelineViewRange', function() {
  netInternalsTest.switchToView('timeline');

  // Set startTime/endTime for sanity checks.
  startTime = graphView.startTime_;
  endTime = graphView.endTime_;
  sanityCheckWithTimeRange(true);

  startTime = 0;
  endTime = 10;
  graphView.setDateRange(new Date(startTime), new Date(endTime));
  sanityCheckWithTimeRange(true);

  endTime = (new Date()).getTime();
  graphView.updateEndDate();

  expectGE(graphView.endTime_, endTime);
  sanityCheck();

  testDone();
});

netInternalsTest.test('netInternalsTimelineViewScrollbar', function() {
  // The range we want the graph to have.
  var expectedGraphRange = canvas.width;

  function checkGraphRange() {
    expectEquals(expectedGraphRange, scrollbar.getRange());
  }

  var taskQueue = new netInternalsTest.TaskQueue(true);
  // Load a log and then switch to the timeline view.  The end time is
  // calculated so that the range is exactly |expectedGraphRange|.
  taskQueue.addTask(
      new LoadLogWithNewEventsTask(
          55, 55 + graphView.scale_ * (canvas.width + expectedGraphRange)));
  taskQueue.addFunctionTask(
      netInternalsTest.switchToView.bind(null, 'timeline'));
  taskQueue.addFunctionTask(checkGraphRange);

  taskQueue.addTask(new MouseScrollTask(0));
  taskQueue.addTask(new MouseScrollTask(expectedGraphRange));
  taskQueue.addTask(new MouseScrollTask(1));
  taskQueue.addTask(new MouseScrollTask(expectedGraphRange - 1));

  taskQueue.addFunctionTask(checkGraphRange);
  taskQueue.addFunctionTask(sanityCheckWithTimeRange.bind(null, false));
  taskQueue.run();
});

netInternalsTest.test('netInternalsTimelineViewLoadLog', function() {
  // After loading the log file, the rest of the test runs synchronously.
  function testBody() {
    netInternalsTest.switchToView('timeline');
    sanityCheckWithTimeRange(false);

    // Make sure everything's still fine when we switch to another view.
    netInternalsTest.switchToView('events');
    sanityCheckWithTimeRange(false);
  }

  // Load a log and then run the rest of the test.
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new LoadLogWithNewEventsTask(55, 10055));
  taskQueue.addFunctionTask(testBody);
  taskQueue.run();
});

netInternalsTest.test('netInternalsTimelineViewZoomOut', function() {
  // After loading the log file, the rest of the test runs synchronously.
  function testBody() {
    netInternalsTest.switchToView('timeline');
    mouseZoomOut();
    mouseZoomOut();
    mouseZoomIn();
    sanityCheckWithTimeRange(false);
  }

  // Load a log and then run the rest of the test.
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new LoadLogWithNewEventsTask(55, 10055));
  taskQueue.addFunctionTask(testBody);
  taskQueue.run();
});

netInternalsTest.test('netInternalsTimelineViewZoomIn', function() {
  // After loading the log file, the rest of the test runs synchronously.
  function testBody() {
    netInternalsTest.switchToView('timeline');
    mouseZoomAllTheWayIn();
    mouseZoomOut();
    sanityCheckWithTimeRange(false);
  }

  // Load a log and then run the rest of the test.
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new LoadLogWithNewEventsTask(55, 10055));
  taskQueue.addFunctionTask(testBody);
  taskQueue.run();
});

netInternalsTest.test('netInternalsTimelineViewDegenerate', function() {
  // After loading the log file, the rest of the test runs synchronously.
  function testBody() {
    netInternalsTest.switchToView('timeline');
    mouseZoomOut();
    mouseZoomAllTheWayIn();
    mouseZoomOut();
    sanityCheckWithTimeRange(false);
  }

  // Load a log and then run the rest of the test.
  var taskQueue = new netInternalsTest.TaskQueue(true);
  taskQueue.addTask(new LoadLogWithNewEventsTask(55, 10055));
  taskQueue.addFunctionTask(testBody);
  taskQueue.run();
});

/**
 * Since we don't need to load a log file, this test can run synchronously.
 */
netInternalsTest.test('netInternalsTimelineViewNoEvents', function() {
  // Click the events view's delete all button, and then switch to timeline
  // view.
  netInternalsTest.switchToView('events');
  $(EventsView.DELETE_ALL_ID).click();
  netInternalsTest.switchToView('timeline');

  // Set startTime/endTime for sanity checks.
  startTime = graphView.startTime_;
  endTime = graphView.endTime_;

  sanityCheckWithTimeRange(true);

  mouseZoomOut();
  sanityCheckWithTimeRange(true);

  mouseZoomAllTheWayIn();
  sanityCheckWithTimeRange(true);

  mouseZoomOut();
  sanityCheckWithTimeRange(true);

  testDone();
});

})();  // Anonymous namespace
