/*
  Copyright 2010 Google

  Licensed under the Apache License, Version 2.0 (the "License"); you may not
  use this file except in compliance with the License. You may obtain a copy of
  the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
  License for the specific language governing permissions and limitations under
  the License.

  Brian Kennish <byoogle@google.com>
*/
const ELLIPSIS = ['   ', '   ', '  .', '  .', ' ..', ' ..', '...', '...'];
const SPINNER = ['|', '/', '-', '\\'];
const BOUNCING_BALL = [
  '    0', '   o ', '  _  ', ' o   ', '0    ', ' o   ', '  _  ', '   o '
];
const SINE_WAVE = ['.::.', '::..', ':..:', '..::'];
const TRIANGLE_WAVE = ['/\\/\\', '\\/\\/'];

function Prettyload(
  frames,
  backgroundColor,
  successBackgroundColor,
  timeoutBackgroundColor,
  frameRate,
  minDuration,
  maxDuration
) {
  const BROWSER_ACTION = chrome.browserAction;
  var startTime;
  const MS_PER_SEC = 1000;
  var id;
  this.frames = frames !== undefined ? frames : ELLIPSIS;
  this.backgroundColor = backgroundColor !== undefined ? backgroundColor : null;
  this.successBackgroundColor =
    successBackgroundColor !== undefined ? successBackgroundColor : null;
  this.timeoutBackgroundColor =
    timeoutBackgroundColor !== undefined ? timeoutBackgroundColor : null;
  this.frameRate = frameRate !== undefined ? frameRate : 12;
  this.minDuration = minDuration !== undefined ? minDuration : 1;
  this.maxDuration = maxDuration !== undefined ? maxDuration : 10;

  function finish(frame, backgroundColor) {
    clearInterval(id);

    if (backgroundColor) {
      BROWSER_ACTION.setBadgeBackgroundColor({color: backgroundColor});
    }

    BROWSER_ACTION.setBadgeText({text: frame});
  }

  this.start = function(timeoutFrame) {
    function timeout() {
      finish(
        timeoutFrame !== undefined ? timeoutFrame : '', TIMEOUT_BACKGROUND_COLOR
      );
    }

    if (this.backgroundColor) {
      BROWSER_ACTION.setBadgeBackgroundColor({color: this.backgroundColor});
    }

    startTime = Date.now();
    const MAX_DURATION = this.maxDuration * MS_PER_SEC;
    const FRAMES = this.frames;
    var i = 0;
    const FRAME_COUNT = FRAMES.length;
    const TIMEOUT_BACKGROUND_COLOR = this.timeoutBackgroundColor;

    if (0 < MAX_DURATION) {
      BROWSER_ACTION.setBadgeText({text: FRAMES[i]});

      id = setInterval(function() {
        if (Date.now() - startTime < MAX_DURATION) {
          BROWSER_ACTION.setBadgeText({text: FRAMES[++i % FRAME_COUNT]});
        } else { timeout(); }
      }, MS_PER_SEC / this.frameRate);
    } else { timeout(); }

    return this;
  };

  this.finish = function(successFrame) {
    const SUCCESS_BACKGROUND_COLOR = this.successBackgroundColor;

    setTimeout(function() {
      finish(
        successFrame !== undefined ? successFrame : '', SUCCESS_BACKGROUND_COLOR
      );
    }, this.minDuration * MS_PER_SEC - (Date.now() - startTime));

    return this;
  };
}
