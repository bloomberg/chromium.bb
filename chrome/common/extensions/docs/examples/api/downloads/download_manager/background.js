// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (chrome.downloads.setShelfEnabled)
  chrome.downloads.setShelfEnabled(false);

var colors = {
  progressColor: '#0d0',
  arrow: '#555',
  danger: 'red',
  complete: 'green',
  paused: 'grey',
  background: 'white',
};

function drawLine(ctx, x1, y1, x2, y2) {
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
}

function drawProgressSpinner(ctx, stage) {
  var center = ctx.canvas.width/2;
  var radius = center*0.9;
  const segments = 16;
  var segArc = 2 * Math.PI / segments;
  ctx.lineWidth = Math.round(ctx.canvas.width*0.1);
  for (var seg = 0; seg < segments; ++seg) {
    var color = ((stage == 'm') ? ((seg % 2) == 0) : (seg <= stage));
    ctx.fillStyle = ctx.strokeStyle = (
      color ? colors.progressColor : colors.background);
    ctx.moveTo(center, center);
    ctx.beginPath();
    ctx.arc(center, center, radius, (seg-4)*segArc, (seg-3)*segArc, false);
    ctx.lineTo(center, center);
    ctx.fill();
    ctx.stroke();
  }
  ctx.strokeStyle = colors.background;
  radius += ctx.lineWidth-1;
  drawLine(ctx, center-radius, center, center+radius, center);
  drawLine(ctx, center, center-radius, center, center+radius);
  radius *= Math.sin(Math.PI/4);
  drawLine(ctx, center-radius, center-radius, center+radius, center+radius);
  drawLine(ctx, center-radius, center+radius, center+radius, center-radius);
}

function drawArrow(ctx) {
  ctx.beginPath();
  ctx.lineWidth = Math.round(ctx.canvas.width*0.1);
  ctx.lineJoin = 'round';
  ctx.strokeStyle = ctx.fillStyle = colors.arrow;
  var center = ctx.canvas.width/2;
  var minw2 = center*0.2;
  var maxw2 = center*0.60;
  var height2 = maxw2;
  ctx.moveTo(center-minw2, center-height2);
  ctx.lineTo(center+minw2, center-height2);
  ctx.lineTo(center+minw2, center);
  ctx.lineTo(center+maxw2, center);
  ctx.lineTo(center, center+height2);
  ctx.lineTo(center-maxw2, center);
  ctx.lineTo(center-minw2, center);
  ctx.lineTo(center-minw2, center-height2);
  ctx.lineTo(center+minw2, center-height2);
  ctx.stroke();
  ctx.fill();
}

function drawDangerBadge(ctx) {
  var s = ctx.canvas.width/100;
  ctx.fillStyle = colors.danger;
  ctx.strokeStyle = colors.background;
  ctx.lineWidth = Math.round(s*5);
  var edge = ctx.canvas.width-ctx.lineWidth;
  ctx.beginPath();
  ctx.moveTo(s*75, s*55);
  ctx.lineTo(edge, edge);
  ctx.lineTo(s*55, edge);
  ctx.lineTo(s*75, s*55);
  ctx.lineTo(edge, edge);
  ctx.fill();
  ctx.stroke();
}

function drawPausedBadge(ctx) {
  var s = ctx.canvas.width/100;
  ctx.beginPath();
  ctx.strokeStyle = colors.background;
  ctx.lineWidth = Math.round(s*5);
  ctx.rect(s*55, s*55, s*15, s*35);
  ctx.fillStyle = colors.paused;
  ctx.fill();
  ctx.stroke();
  ctx.rect(s*75, s*55, s*15, s*35);
  ctx.fill();
  ctx.stroke();
}

function drawCompleteBadge(ctx) {
  var s = ctx.canvas.width/100;
  ctx.beginPath();
  ctx.arc(s*75, s*75, s*15, 0, 2*Math.PI, false);
  ctx.fillStyle = colors.complete;
  ctx.fill();
  ctx.strokeStyle = colors.background;
  ctx.lineWidth = Math.round(s*5);
  ctx.stroke();
}

function drawIcon(side, stage, badge) {
  var canvas = document.createElement('canvas');
  canvas.width = canvas.height = side;
  document.body.appendChild(canvas);
  var ctx = canvas.getContext('2d');
  if (stage != 'n') {
    drawProgressSpinner(ctx, stage);
  }
  drawArrow(ctx);
  if (badge == 'd') {
    drawDangerBadge(ctx);
  } else if (badge == 'p') {
    drawPausedBadge(ctx);
  } else if (badge == 'c') {
    drawCompleteBadge(ctx);
  }
  return canvas;
}

function maybeOpen(id) {
  var openWhenComplete = [];
  try {
    openWhenComplete = JSON.parse(localStorage.openWhenComplete);
  } catch (e) {
    localStorage.openWhenComplete = JSON.stringify(openWhenComplete);
  }
  var openNowIndex = openWhenComplete.indexOf(id);
  if (openNowIndex >= 0) {
    chrome.downloads.open(id);
    openWhenComplete.splice(openNowIndex, 1);
    localStorage.openWhenComplete = JSON.stringify(openWhenComplete);
  }
}

function setBrowserActionIcon(stage, badge) {
  var canvas1 = drawIcon(19, stage, badge);
  var canvas2 = drawIcon(38, stage, badge);
  var imageData = {};
  imageData['' + canvas1.width] = canvas1.getContext('2d').getImageData(
        0, 0, canvas1.width, canvas1.height);
  imageData['' + canvas2.width] = canvas2.getContext('2d').getImageData(
        0, 0, canvas2.width, canvas2.height);
  chrome.browserAction.setIcon({imageData:imageData});
  canvas1.parentNode.removeChild(canvas1);
  canvas2.parentNode.removeChild(canvas2);
}

function pollProgress() {
  pollProgress.tid = -1;
  chrome.downloads.search({}, function(items) {
    var popupLastOpened = parseInt(localStorage.popupLastOpened);
    var totalTotalBytes = 0;
    var totalBytesReceived = 0;
    var anyMissingTotalBytes = false;
    var anyDangerous = false;
    var anyPaused = false;
    var anyRecentlyCompleted = false;
    var anyInProgress = false;
    items.forEach(function(item) {
      if (item.state == 'in_progress') {
        anyInProgress = true;
        if (item.totalBytes) {
          totalTotalBytes += item.totalBytes;
          totalBytesReceived += item.bytesReceived;
        } else {
          anyMissingTotalBytes = true;
        }
        var dangerous = ((item.danger != 'safe') &&
                         (item.danger != 'accepted'));
        anyDangerous = anyDangerous || dangerous;
        anyPaused = anyPaused || item.paused;
      } else if ((item.state == 'complete') && item.endTime && !item.error) {
        var ended = (new Date(item.endTime)).getTime();
        var recentlyCompleted = (ended >= popupLastOpened);
        anyRecentlyCompleted = anyRecentlyCompleted || recentlyCompleted;
        maybeOpen(item.id);
      }
    });
    var stage = !anyInProgress ? 'n' : (anyMissingTotalBytes ? 'm' :
      parseInt((totalBytesReceived / totalTotalBytes) * 15));
    var badge = anyDangerous ? 'd' : (anyPaused ? 'p' :
      (anyRecentlyCompleted ? 'c' : ''));

    var targetIcon = stage + ' ' + badge;
    if (sessionStorage.currentIcon != targetIcon) {
      setBrowserActionIcon(stage, badge);
      sessionStorage.currentIcon = targetIcon;
    }

    if (anyInProgress &&
        (pollProgress.tid < 0)) {
      pollProgress.start();
    }
  });
}
pollProgress.tid = -1;
pollProgress.MS = 200;

pollProgress.start = function() {
  if (pollProgress.tid < 0) {
    pollProgress.tid = setTimeout(pollProgress, pollProgress.MS);
  }
};

function isNumber(n) {
  return !isNaN(parseFloat(n)) && isFinite(n);
}

if (!isNumber(localStorage.popupLastOpened)) {
  localStorage.popupLastOpened = '' + (new Date()).getTime();
}

chrome.downloads.onCreated.addListener(function(item) {
  pollProgress();
});

pollProgress();

function openWhenComplete(downloadId) {
  var ids = [];
  try {
    ids = JSON.parse(localStorage.openWhenComplete);
  } catch (e) {
    localStorage.openWhenComplete = JSON.stringify(ids);
  }
  pollProgress.start();
  if (ids.indexOf(downloadId) >= 0) {
    return;
  }
  ids.push(downloadId);
  localStorage.openWhenComplete = JSON.stringify(ids);
}

chrome.runtime.onMessage.addListener(function(request) {
  if (request == 'poll') {
    pollProgress.start();
  }
  if (request == 'icons') {
    [16, 19, 38, 128].forEach(function(s) {
      var canvas = drawIcon(s, 'n', '');
      chrome.downloads.download({
        url: canvas.toDataURL('image/png', 1.0),
        filename: 'icon' + s + '.png',
      });
      canvas.parentNode.removeChild(canvas);
    });
  }
  if (isNumber(request.openWhenComplete)) {
    openWhenComplete(request.openWhenComplete);
  }
});
