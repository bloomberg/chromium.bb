// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview TimelineView visualizes TRACE_EVENT events using the
 * tracing.Timeline component and adds in selection summary and control buttons.
 */
cr.define('tracing', function() {
  function tsRound(ts) {
    return Math.round(ts * 1000.0) / 1000.0;
  }
  function getPadding(text, width) {
    width = width || 0;

    if (typeof text != 'string')
      text = String(text);

    if (text.length >= width)
      return '';

    var pad = '';
    for (var i = 0; i < width - text.length; i++)
      pad += ' ';
    return pad;
  }

  function leftAlign(text, width) {
    return text + getPadding(text, width);
  }

  function rightAlign(text, width) {
    return getPadding(text, width) + text;
  }

  /**
   * TimelineFindControl
   * @constructor
   * @extends {tracing.Overlay}
   */
  var TimelineFindControl = cr.ui.define('div');

  TimelineFindControl.prototype = {
    __proto__: tracing.Overlay.prototype,

    decorate: function() {
      tracing.Overlay.prototype.decorate.call(this);

      this.className = 'timeline-find-control';

      this.hitCountEl_ = document.createElement('span');
      this.hitCountEl_.className = 'hit-count-label';
      this.hitCountEl_.textContent = '1 of 7';

      var findPreviousBn = document.createElement('span');
      findPreviousBn.className = 'find-button find-previous';
      findPreviousBn.textContent = '\u2190';
      findPreviousBn.addEventListener('click', function() {
        this.controller.findPrevious();
        this.updateHitCountEl_();
      }.bind(this));

      var findNextBn = document.createElement('span');
      findNextBn.className = 'find-button find-next';
      findNextBn.textContent = '\u2192';
      findNextBn.addEventListener('click', function() {
        this.controller.findNext();
        this.updateHitCountEl_();
      }.bind(this));

      // Filter input element.
      this.filterEl_ = document.createElement('input');
      this.filterEl_.type = 'input';

      this.filterEl_.addEventListener('input', function(e) {
        this.controller.filterText = this.filterEl_.value;
        this.updateHitCountEl_();
      }.bind(this));

      this.filterEl_.addEventListener('keydown', function(e) {
        if (e.keyCode == 13) {
          findNextBn.click();
        } else if (e.keyCode == 27) {
          this.filterEl_.blur();
          this.updateHitCountEl_();
        }
      }.bind(this));

      this.filterEl_.addEventListener('blur', function(e) {
        this.updateHitCountEl_();
      }.bind(this));

      this.filterEl_.addEventListener('focus', function(e) {
        this.updateHitCountEl_();
      }.bind(this));

      // Attach everything.
      this.appendChild(this.filterEl_);

      this.appendChild(findPreviousBn);
      this.appendChild(findNextBn);
      this.appendChild(this.hitCountEl_);

      this.updateHitCountEl_();
    },

    get controller() {
      return this.controller_;
    },

    set controller(c) {
      this.controller_ = c;
      this.updateHitCountEl_();
    },

    focus: function() {
      this.filterEl_.selectionStart = 0;
      this.filterEl_.selectionEnd = this.filterEl_.value.length;
      this.filterEl_.focus();
    },

    updateHitCountEl_: function() {
      if (!this.controller || document.activeElement != this.filterEl_) {
        this.hitCountEl_.textContent = '';
        return;
      }
      var i = this.controller.currentHitIndex;
      var n = this.controller.filterHits.length;
      if (n == 0)
        this.hitCountEl_.textContent = '0 of 0';
      else
        this.hitCountEl_.textContent = (i + 1) + ' of ' + n;
    }
  };

  function TimelineFindController() {
    this.timeline_ = undefined;
    this.model_ = undefined;
    this.filterText_ = '';
    this.filterHitsDirty_ = true;
    this.currentHitIndex_ = 0;
  };

  TimelineFindController.prototype = {
    __proto__: Object.prototype,

    get timeline() {
      return this.timeline_;
    },

    set timeline(t) {
      this.timeline_ = t;
      this.filterHitsDirty_ = true;
    },

    get filterText() {
      return this.filterText_;
    },

    set filterText(f) {
      if (f == this.filterText_)
        return;
      this.filterText_ = f;
      this.filterHitsDirty_ = true;
      this.findNext();
    },

    get filterHits() {
      if (this.filterHitsDirty_) {
        this.filterHitsDirty_ = false;
        if (this.timeline_) {
          var filter = new tracing.TimelineFilter(this.filterText);
          this.filterHits_ = this.timeline.findAllObjectsMatchingFilter(filter);
          this.currentHitIndex_ = this.filterHits_.length - 1;
        } else {
          this.filterHits_ = [];
          this.currentHitIndex_ = 0;
        }
      }
      return this.filterHits_;
    },

    get currentHitIndex() {
      return this.currentHitIndex_;
    },

    find_: function(dir) {
      if (!this.timeline)
        return;

      var N = this.filterHits.length;
      this.currentHitIndex_ = this.currentHitIndex_ + dir;

      if (this.currentHitIndex_ < 0) this.currentHitIndex_ = N - 1;
      if (this.currentHitIndex_ >= N) this.currentHitIndex_ = 0;

      if (this.currentHitIndex_ < 0 || this.currentHitIndex_ >= N) {
        this.timeline.selection = [];
        return;
      }

      var hit = this.filterHits[this.currentHitIndex_];

      // We allow the zoom level to change on the first hit level. But, when
      // then cycling through subsequent changes, restrict it to panning.
      var zoomAllowed = this.currentHitIndex_ == 0;
      this.timeline.setSelectionAndMakeVisible([hit], zoomAllowed);
    },

    findNext: function() {
      this.find_(1);
    },

    findPrevious: function() {
      this.find_(-1);
    },
  };

  /**
   * TimelineView
   * @constructor
   * @extends {HTMLDivElement}
   */
  var TimelineView = cr.ui.define('div');

  TimelineView.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.classList.add('timeline-view');

      // Create individual elements.
      this.titleEl_ = document.createElement('span');
      this.titleEl_.textContent = 'Tracing: ';

      this.controlDiv_ = document.createElement('div');
      this.controlDiv_.className = 'control';

      this.leftControlsEl_ = document.createElement('div');
      this.rightControlsEl_ = document.createElement('div');

      var spacingEl = document.createElement('div');
      spacingEl.className = 'spacer';

      this.timelineContainer_ = document.createElement('div');
      this.timelineContainer_.className = 'timeline-container';

      var summaryContainer_ = document.createElement('div');
      summaryContainer_.className = 'summary-container';

      this.summaryEl_ = document.createElement('pre');
      this.summaryEl_.className = 'summary';

      this.findCtl_ = new TimelineFindControl();
      this.findCtl_.controller = new TimelineFindController();

      // Connect everything up.
      this.rightControls.appendChild(this.findCtl_);
      this.controlDiv_.appendChild(this.titleEl_);
      this.controlDiv_.appendChild(this.leftControlsEl_);
      this.controlDiv_.appendChild(spacingEl);
      this.controlDiv_.appendChild(this.rightControlsEl_);
      this.appendChild(this.controlDiv_);

      this.appendChild(this.timelineContainer_);

      summaryContainer_.appendChild(this.summaryEl_);
      this.appendChild(summaryContainer_);

      // Bookkeeping.
      this.onSelectionChangedBoundToThis_ = this.onSelectionChanged_.bind(this);
      document.addEventListener('keypress', this.onKeypress_.bind(this), true);
    },

    get leftControls() {
      return this.leftControlsEl_;
    },

    get rightControls() {
      return this.rightControlsEl_;
    },

    get title() {
      return this.titleEl_.textContent.substring(
        this.titleEl_.textContent.length - 2);
    },

    set title(text) {
      this.titleEl_.textContent = text + ':';
    },

    set traceData(traceData) {
      this.model = new tracing.TimelineModel(traceData);
    },

    get model() {
      return this.timelineModel_;
    },

    set model(model) {
      this.timelineModel_ = model;

      // remove old timeline
      this.timelineContainer_.textContent = '';

      // create new timeline if needed
      if (this.timelineModel_.minTimestamp !== undefined) {
        if (this.timeline_)
          this.timeline_.detach();
        this.timeline_ = new tracing.Timeline();
        this.timeline_.model = this.timelineModel_;
        this.timeline_.focusElement =
            this.focusElement_ ? this.focusElement_ : this.parentElement;
        this.timelineContainer_.appendChild(this.timeline_);
        this.timeline_.addEventListener('selectionChange',
                                        this.onSelectionChangedBoundToThis_);

        this.findCtl_.controller.timeline = this.timeline_;
        this.onSelectionChanged_();
      } else {
        this.timeline_ = undefined;
        this.findCtl_.controller.timeline = undefined;
      }
    },

    get timeline() {
      return this.timeline_;
    },

    /**
     * Sets the element whose focus state will determine whether
     * to respond to keybaord input.
     */
    set focusElement(value) {
      this.focusElement_ = value;
      if (this.timeline_)
        this.timeline_.focusElement = value;
    },

    /**
     * @return {Element} The element whose focused state determines
     * whether to respond to keyboard inputs.
     * Defaults to the parent element.
     */
    get focusElement() {
      if (this.focusElement_)
        return this.focusElement_;
      return this.parentElement;
    },

    /**
     * @return {boolean} Whether the current timeline is attached to the
     * document.
     */
    get isAttachedToDocument_() {
      var cur = this;
      while (cur.parentNode)
        cur = cur.parentNode;
      return cur == this.ownerDocument;
    },

    get listenToKeys_() {
      if (!this.isAttachedToDocument_)
        return;
      if (!this.focusElement_)
        return true;
      if (this.focusElement.tabIndex >= 0)
        return document.activeElement == this.focusElement;
      return true;
    },

    onKeypress_: function(e) {
      if (!this.listenToKeys_)
        return;

      if (event.keyCode == 47) {
        this.findCtl_.focus();
        event.preventDefault();
        return;
      }
    },

    beginFind: function() {
      if (this.findInProgress_)
        return;
      this.findInProgress_ = true;
      var dlg = TimelineFindControl();
      dlg.controller = new TimelineFindController();
      dlg.controller.timeline = this.timeline;
      dlg.visible = true;
      dlg.addEventListener('close', function() {
        this.findInProgress_ = false;
      }.bind(this));
      dlg.addEventListener('findNext', function() {
      });
      dlg.addEventListener('findPrevious', function() {
      });
    },

    onSelectionChanged_: function(e) {
      var timeline = this.timeline_;
      var selection = timeline.selection;
      if (!selection.length) {
        var oldScrollTop = this.timelineContainer_.scrollTop;
        this.summaryEl_.textContent = timeline.keyHelp;
        this.timelineContainer_.scrollTop = oldScrollTop;
        return;
      }

      var text = '';
      if (selection.length == 1) {
        var c0Width = 14;
        var slice = selection[0].slice;
        text = 'Selected item:\n';
        text += leftAlign('Title', c0Width) + ': ' + slice.title + '\n';
        text += leftAlign('Start', c0Width) + ': ' +
            tsRound(slice.start) + ' ms\n';
        text += leftAlign('Duration', c0Width) + ': ' +
            tsRound(slice.duration) + ' ms\n';
        if (slice.durationInUserTime)
          text += leftAlign('Duration (U)', c0Width) + ': ' +
              tsRound(slice.durationInUserTime) + ' ms\n';

        var n = 0;
        for (var argName in slice.args) {
          n += 1;
        }
        if (n > 0) {
          text += leftAlign('Args', c0Width) + ':\n';
          for (var argName in slice.args) {
            var argVal = slice.args[argName];
            text += leftAlign(' ' + argName, c0Width) + ': ' + argVal + '\n';
          }
        }
      } else {
        var c0Width = 55;
        var c1Width = 12;
        var c2Width = 5;
        text = 'Selection summary:\n';
        var tsLo = Math.min.apply(Math, selection.map(
            function(s) {return s.slice.start;}));
        var tsHi = Math.max.apply(Math, selection.map(
            function(s) {return s.slice.end;}));

        // compute total selection duration
        var titles = selection.map(function(i) { return i.slice.title; });

        var slicesByTitle = {};
        for (var i = 0; i < selection.length; i++) {
          var slice = selection[i].slice;
          if (!slicesByTitle[slice.title])
            slicesByTitle[slice.title] = {
              slices: []
            };
          slicesByTitle[slice.title].slices.push(slice);
        }
        var totalDuration = 0;
        for (var sliceGroupTitle in slicesByTitle) {
          var sliceGroup = slicesByTitle[sliceGroupTitle];
          var duration = 0;
          for (i = 0; i < sliceGroup.slices.length; i++)
            duration += sliceGroup.slices[i].duration;
          totalDuration += duration;

          text += ' ' +
              leftAlign(sliceGroupTitle, c0Width) + ': ' +
              rightAlign(tsRound(duration) + 'ms', c1Width) + '   ' +
              rightAlign(String(sliceGroup.slices.length), c2Width) +
              ' occurrences' + '\n';
        }

        text += leftAlign('*Totals', c0Width) + ' : ' +
            rightAlign(tsRound(totalDuration) + 'ms', c1Width) + '   ' +
            rightAlign(String(selection.length), c2Width) + ' occurrences' +
            '\n';

        text += '\n';

        text += leftAlign('Selection start', c0Width) + ' : ' +
            rightAlign(tsRound(tsLo) + 'ms', c1Width) +
            '\n';
        text += leftAlign('Selection extent', c0Width) + ' : ' +
            rightAlign(tsRound(tsHi - tsLo) + 'ms', c1Width) +
            '\n';
      }

      // done
      var oldScrollTop = this.timelineContainer_.scrollTop;
      this.summaryEl_.textContent = text;
      this.timelineContainer_.scrollTop = oldScrollTop;
    }
  };

  return {
    TimelineFindControl: TimelineFindControl,
    TimelineFindController: TimelineFindController,
    TimelineView: TimelineView
  };
});
