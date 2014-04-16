// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('serviceworker', function() {
    'use strict';

    function update() {
        chrome.send('getAllRegistrations');
    }

    function progressNodeFor(link) {
        return link.parentNode.querySelector('.operation-status');
    }

    // All commands are sent with the partition_path and scope, and
    // are all completed with 'onOperationComplete'.
    var COMMANDS = ['unregister', 'start', 'stop', 'sync'];
    function commandHandler(command) {
        return function(event) {
            var link = event.target;
            progressNodeFor(link).style.display = 'inline';
            chrome.send(command, [link.partition_path,
                                  link.scope]);
            return false;
        };
    };

    function withNode(selector, partition_path, scope, callback) {
        var links = document.querySelectorAll(selector);
        for (var i = 0; i < links.length; ++i) {
            var link = links[i];
            if (partition_path == link.partition_path &&
                scope == link.scope) {
                callback(link);
            }
        }
    }

    // Fired from the backend after the start call has completed
    function onOperationComplete(status, path, scope) {
        // refreshes the ui, displaying any relevant buttons
        withNode('button', path, scope, function(link) {
            progressNodeFor(link).style.display = 'none';
        });
        update();
    }

    // Fired once per partition from the backend.
    function onPartitionData(registrations, partition_path) {
        var template;
        var container = $('serviceworker-list');

        // Existing templates are keyed by partition_path. This allows
        // the UI to be updated in-place rather than refreshing the
        // whole page.
        for (var i = 0; i < container.childNodes.length; ++i) {
            if (container.childNodes[i].partition_path == partition_path) {
                template = container.childNodes[i];
            }
        }

        // This is probably the first time we're loading.
        if (!template) {
            template = jstGetTemplate('serviceworker-list-template');
            container.appendChild(template);
        }

        jstProcess(new JsEvalContext({ registrations: registrations,
                                       partition_path: partition_path}),
                   template);
        for (var i = 0; i < COMMANDS.length; ++i) {
            var handler = commandHandler(COMMANDS[i]);
            var links = container.querySelectorAll('button.' + COMMANDS[i]);
            for (var j = 0; j < links.length; ++j) {
                if (!links[j].hasClickEvent) {
                    links[j].addEventListener('click', handler, false);
                    links[j].hasClickEvent = true;
                }
            }
        }
    }

    function onWorkerStarted(version_id, process_id, thread_id) {
        update();
    }

    function onWorkerStopped(version_id, process_id, thread_id) {
        update();
    }

    function onErrorReported(version_id,
                             process_id,
                             thread_id,
                             error_info) {
        var logText = JSON.stringify(error_info);
        var container = $('serviceworker-list');
        var logAreas =
            container.querySelectorAll('textarea.serviceworker-error-log');
        for (var i = 0; i < logAreas.length; ++i) {
            var logArea = logAreas[i];
            if (version_id == logArea.vid && process_id == logArea.pid &&
                thread_id == logArea.tid) {
                logArea.value += logText + '\n';
                return;
            }
        }
        console.error('uncaught error log from version_id:' + version_id +
                      'process_id:' + process_id +
                      'thread_id:' + thread_id);
        console.error(logText);
    }

    function onVersionStateChanged(version_id) {
        update();
    }

    return {
        update: update,
        onOperationComplete: onOperationComplete,
        onPartitionData: onPartitionData,
        onWorkerStarted: onWorkerStarted,
        onWorkerStopped: onWorkerStopped,
        onErrorReported: onErrorReported,
        onVersionStateChanged: onVersionStateChanged,
    };
});

document.addEventListener('DOMContentLoaded', serviceworker.update);
