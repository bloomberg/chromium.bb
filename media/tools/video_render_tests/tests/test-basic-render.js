// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function init(seek_location)
{
    document.addEventListener("canplaythrough", function () {
        var video = document.getElementById('video');
        video.addEventListener("playing", function() {
            video.pause();
            video.addEventListener("seeked", function() {
                document.body.appendChild(document.createTextNode('END OF TEST'));
                if (window.layoutTestController)
                    layoutTestController.notifyDone();
            });
            video.currentTime = seek_location;
        });
        video.play();
    }, true);
}

if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    setTimeout(function() {
        document.body.appendChild(document.createTextNode('FAIL'));
        if (window.layoutTestController)
            layoutTestController.notifyDone();
    } , 8000);
}
