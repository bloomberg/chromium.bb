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

    var allLogMessages = {};

    // Fired once per partition from the backend.
    function onPartitionData(registrations, partition_id, partition_path) {
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

        // Set log for each worker versions.
        if (!(partition_id in allLogMessages)) {
            allLogMessages[partition_id] = {};
        }
        var logMessages = allLogMessages[partition_id];
        registrations.forEach(function (worker) {
            [worker.active, worker.pending].forEach(function (version) {
                if (version) {
                    if (version.version_id in logMessages) {
                        version.log = logMessages[version.version_id];
                    } else {
                        version.log = '';
                    }
                }
            });
        });

        jstProcess(new JsEvalContext({ registrations: registrations,
                                       partition_id: partition_id,
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

    function onWorkerStarted(partition_id, version_id, process_id, thread_id) {
        update();
    }

    function onWorkerStopped(partition_id, version_id, process_id, thread_id) {
        update();
    }

    function onErrorReported(partition_id,
                             version_id,
                             process_id,
                             thread_id,
                             error_info) {
        outputLogMessage(partition_id,
                         version_id,
                         'Error: ' + JSON.stringify(error_info) + '\n');
    }

    function onConsoleMessageReported(partition_id,
                             version_id,
                             process_id,
                             thread_id,
                             message) {
        outputLogMessage(partition_id,
                         version_id,
                         'Console: ' + JSON.stringify(message) + '\n');
    }

    function onVersionStateChanged(partition_id, version_id) {
        update();
    }

    function onRegistrationStored(scope) {
        update();
    }

    function onRegistrationDeleted(scope) {
        update();
    }

    function outputLogMessage(partition_id, version_id, message) {
        if (!(partition_id in allLogMessages)) {
            allLogMessages[partition_id] = {};
        }
        var logMessages = allLogMessages[partition_id];
        if (version_id in logMessages) {
            logMessages[version_id] += message;
        } else {
            logMessages[version_id] = message;
        }

        var logAreas =
            document.querySelectorAll('textarea.serviceworker-log');
        for (var i = 0; i < logAreas.length; ++i) {
            var logArea = logAreas[i];
            if (logArea.partition_id == partition_id &&
                logArea.version_id == version_id) {
                logArea.value += message;
            }
        }
    }

    return {
        update: update,
        onOperationComplete: onOperationComplete,
        onPartitionData: onPartitionData,
        onWorkerStarted: onWorkerStarted,
        onWorkerStopped: onWorkerStopped,
        onErrorReported: onErrorReported,
        onConsoleMessageReported: onConsoleMessageReported,
        onVersionStateChanged: onVersionStateChanged,
        onRegistrationStored: onRegistrationStored,
        onRegistrationDeleted: onRegistrationDeleted,
    };
});

document.addEventListener('DOMContentLoaded', serviceworker.update);
