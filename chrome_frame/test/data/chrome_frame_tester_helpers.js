//
// This script provides some mechanics for testing ChromeFrame
//
function onSuccess(name, id) {
  appendStatus("Success reported!");
  onFinished(name, id, "OK");
}

function onFailure(name, id, status) {
  appendStatus("Failure reported: " + status);
  onFinished(name, id, status);
}

function byId(id) {
  return document.getElementById(id);
}

function getXHRObject(){
  var XMLHTTP_PROGIDS = ['Msxml2.XMLHTTP', 'Microsoft.XMLHTTP',
                         'Msxml2.XMLHTTP.4.0'];
  var http = null;
  try {
    http = new XMLHttpRequest();
  } catch(e) {
  }

  if (http)
    return http;

  for (var i = 0; i < 3; ++i) {
    var progid = XMLHTTP_PROGIDS[i];
    try {
      http = new ActiveXObject(progid);
    } catch(e) {
  }

  if (http)
    break;
  }
  return http;
}

var reportURL = "/writefile/";

// Optionally send the server a notification that onload was fired.
// To be called from within window.onload.
function sendOnLoadEvent() {
  writeToServer("OnLoadEvent", "loaded");
}

function writeToServer(name, result) {
  var xhr = getXHRObject();
  if(!xhr)
    return;

  // asynchronously POST the results
  xhr.open("POST", reportURL + name, true);
  xhr.setRequestHeader("X-Requested-With", "XMLHttpRequest");
  try {
    xhr.send(result);
  } catch(e) {
    appendStatus("XHR send failed. Error: " + e.description);
  }
}

function postResult(name, result) {
  writeToServer(name, result);
}

// Finish running a test by setting the status
// and the cookie.
function onFinished(name, id, result) {
  // set a cookie to report the results...
  var cookie = name + "." + id + ".status=" + result + "; path=/";
  document.cookie = cookie;

  // ...and POST the status back to the server
  postResult(name, result);
}

function appendStatus(message) {
  var statusPanel = byId("statusPanel");
  if (statusPanel) {
    statusPanel.innerHTML += '<BR>' + message;
  }
}

function readCookie(name) {
  var cookie_name = name + "=";
  var ca = document.cookie.split(';');

  for(var i = 0 ; i < ca.length ; i++) {
    var c = ca[i];
    while (c.charAt(0) == ' ') {
      c = c.substring(1,c.length);
    }
    if (c.indexOf(cookie_name) == 0) {
      return c.substring(cookie_name.length, c.length);
    }
  }
  return null;
}

function createCookie(name,value,days) {
  var expires = "";
  if (days) {
    var date = new Date();
    date.setTime(date.getTime() + (days * 24 * 60 * 60 * 1000));
    expires = "; expires=" + date.toGMTString();
  }
  document.cookie = name+"="+value+expires+"; path=/";
}

function eraseCookie(name) {
  createCookie(name, "", -1);
}

function isRunningInMSIE() {
  if (/MSIE (\d+\.\d+);/.test(navigator.userAgent))
      return true;

  return false;
}

function reloadUsingCFProtocol() {
  var redirect_location = "gcf:";
  redirect_location += window.location;
  window.location = redirect_location;
}

function isRunningInChrome() {
  var is_chrome_frame = /chromeframe/.test(navigator.userAgent.toLowerCase());
  if (is_chrome_frame)
    return 0;
  var is_chrome = /chrome/.test(navigator.userAgent.toLowerCase());
  return is_chrome;
}

function TestIfRunningInChrome() {
  var is_chrome = isRunningInChrome();
  if (!is_chrome) {
    onFailure("ChromeFrameWindowOpen", "Window Open failed :-(",
              "User agent = " + navigator.userAgent.toLowerCase());
  }
  return is_chrome;
}

// Returns the base document url.
function GetBaseUrlPath() {
 var url = window.location.protocol + "//" + window.location.host + "/";
 return url;
}

// Appends arguments passed in to the base document url and returns the same.
function AppendArgumentsToBaseUrl() {
  var url = GetBaseUrlPath();
  for (arg_index = 0; arg_index < arguments.length; arg_index++) {
    url += arguments[arg_index];
  }
  return url;
}
