// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var lkgrURL = 'http://chromium-status.appspot.com/lkgr';

function getClassForTryJobResult(result) {
  // Some win bots seem to report a null result while building.
  if (result === null)
    result = RUNNING;

  switch (parseInt(result)) {
  case RUNNING:
    return "running";

  case SUCCESS:
    return "success";

  case WARNINGS:
    return "warnings";

  case FAILURE:
    return "failure";

  case SKIPPED:
    return "skipped";

  case EXCEPTION:
    return "exception";

  case RETRY:
    return "retry";

  case NOT_STARTED:
  default:
    return "never";
  }
}

// Remove try jobs that have been supplanted by newer runs.
function filterOldTryJobs(tryJobs) {
  var latest = {};
  tryJobs.forEach(function(tryJob) {
    if (!latest[tryJob.builder] ||
        latest[tryJob.builder].buildnumber < tryJob.buildnumber)
      latest[tryJob.builder] = tryJob;
  });

  var result = [];
  tryJobs.forEach(function(tryJob) {
    if (tryJob.buildnumber == latest[tryJob.builder].buildnumber)
      result.push(tryJob);
  });

  return result;
}

function createTryJobAnchorTitle(tryJob, fullTryJob) {
  var title = tryJob.builder;

  if (!fullTryJob)
    return title;

  var stepText = [];
  if (fullTryJob.currentStep)
    stepText.push("running " + fullTryJob.currentStep.name);

  if (fullTryJob.results == FAILURE && fullTryJob.text) {
    stepText.push(fullTryJob.text.join(" "));
  } else {
    // Sometimes a step can fail without setting the try job text.  Look
    // through all the steps to identify if this is the case.
    var text = [];
    fullTryJob.steps.forEach(function(step) {
      if (step.results[0] == FAILURE)
        text.push(step.results[1][0]);
    });

    if (text.length > 0) {
      text.unshift("failure");
      stepText.push(text.join(" "));
    }
  }

  if (stepText.length > 0)
    title += ": " + stepText.join("; ");

  return title;
}

// Create an iframe mimicking the horizontal_one_box_per_builder format and
// reuse its CSS, to get the same visual styling.
function createPatchsetStatusElement(patchset) {
  var cssURL = "http://chromium-build.appspot.com/p/chromium/default.css";
  var content =
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" " +
    "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\
    <html>\
  <head>\
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\
    <link href=\"" + cssURL + "\" rel=\"stylesheet\" type=\"text/css\" />\
    <style>td {vertical-align: bottom;}</style>\
  </head>\
  <body vlink=\"#800080\">\
    <table style=\"width: 100%\"><tr id=\"bot-table-row\"></tr></table>\
  </body>\
</html>";
  var doc = (new DOMParser()).parseFromString(content, "text/xml");
  var row = doc.getElementById("bot-table-row");

  var tryJobs = filterOldTryJobs(patchset.try_job_results);
  tryJobs.forEach(function(tryJob) {
    var cell = doc.createElement("td");
    cell.className = "mini-box";
    row.appendChild(cell);

    var key = tryJob.builder + "-" + tryJob.buildnumber;
    var fullTryJob = patchset.full_try_job_results &&
        patchset.full_try_job_results[key];

    var tryJobAnchor = document.createElement("a");
    tryJobAnchor.textContent = "  ";
    tryJobAnchor.title = createTryJobAnchorTitle(tryJob, fullTryJob);
    tryJobAnchor.className = "LastBuild " +
      getClassForTryJobResult(tryJob.result);
    tryJobAnchor.target = "_blank";
    tryJobAnchor.href = tryJob.url;
    cell.appendChild(tryJobAnchor);
  });

  var iframe = document.createElement("iframe");
  iframe.className="statusiframe";
  iframe.scrolling="no";
  iframe.srcdoc = (new XMLSerializer).serializeToString(doc);
  return iframe;
}

function addTryStatusRows(table) {
  var codereviewBaseURL = "https://codereview.chromium.org";

  var background = chrome.extension.getBackgroundPage();

  var issues = [];
  for (var key in background.activeIssues)
    issues.push(background.activeIssues[key]);

  // Sort issues in descending order.
  issues.sort(function(a, b) {return parseInt(b.issue) - parseInt(a.issue);});

  issues.forEach(function(issue) {
    var codereviewURL = codereviewBaseURL + "/" + issue.issue;

    if (!issue.full_patchsets)
      return;

    var lastPatchset = issue.patchsets[issue.patchsets.length - 1];
    var lastFullPatchset = issue.full_patchsets[lastPatchset];

    if (!lastFullPatchset.try_job_results ||
        lastFullPatchset.try_job_results.length == 0)
      return;

    var row = table.insertRow(-1);
    var label = row.insertCell(-1);
    label.className = "status-label";
    var clAnchor = document.createElement("a");
    clAnchor.textContent = "CL " + issue.issue;
    clAnchor.href = codereviewURL;
    clAnchor.title = issue.subject;
    if (lastFullPatchset.message)
      clAnchor.title += " | " + lastFullPatchset.message;
    clAnchor.target = "_blank";
    label.appendChild(clAnchor);

    var status = row.insertCell(-1);
    status.appendChild(createPatchsetStatusElement(lastFullPatchset));
  });

  if (issues.length > 0)
    table.insertRow(-1).insertCell().className = "spacer";
}

function updateLKGR(lkgr) {
  var link = document.getElementById('link_lkgr');
  link.textContent = 'LKGR (' + lkgr + ')';
}

function addBotStatusRow(table, bot) {
  var baseURL = "http://build.chromium.org/p/chromium" +
    (bot.id != "" ? "." + bot.id : "");
  var consoleURL = baseURL + "/console";
  var statusURL = baseURL + "/horizontal_one_box_per_builder";

  var row = table.insertRow(-1);
  var label = row.insertCell(-1);
  label.className = "status-label";
  var labelAnchor = document.createElement("a");
  labelAnchor.href = consoleURL;
  labelAnchor.target = "_blank";
  labelAnchor.id = "link_" + bot.id;
  labelAnchor.textContent = bot.label;
  label.appendChild(labelAnchor);

  var status = row.insertCell(-1);
  status.className = "botstatus";
  var statusIframe = document.createElement("iframe");
  statusIframe.className = "statusiframe";
  statusIframe.scrolling = "no";
  statusIframe.src = statusURL;
  status.appendChild(statusIframe);
}

function addBotStatusRows(table) {
  var closerBots = [
    {id: "", label: "Chromium"},
    {id: "win", label: "Win"},
    {id: "mac", label: "Mac"},
    {id: "linux", label: "Linux"},
    {id: "chromiumos", label: "ChromiumOS"},
    {id: "chrome", label: "Official"},
    {id: "memory", label: "Memory"}
  ];

  var otherBots = [
    {id: "lkgr", label: "LKGR"},
    {id: "perf", label: "Perf"},
    {id: "memory.fyi", label: "Memory FYI"},
    {id: "gpu", label: "GPU"},
    {id: "gpu.fyi", label: "GPU FYI"}
  ];

  closerBots.forEach(function(bot) {
    addBotStatusRow(table, bot);
  });

  table.insertRow(-1).insertCell().className = "spacer";

  otherBots.forEach(function(bot) {
    addBotStatusRow(table, bot);
  });
}

function fillStatusTable() {
  var table = document.getElementById("status-table");
  addTryStatusRows(table);
  addBotStatusRows(table);
}

function main() {
  requestURL(lkgrURL, "text", updateLKGR);
  fillStatusTable();
}

main();
