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

// Get the value of the first parameter from the query string which matches the
// given name.
function getURLParameter(name) {
  name = name.replace(/[\[]/, "\\\[").replace(/[\]]/, "\\\]");
  var regexString = "[\\?&]" + name + "=([^&#]*)";
  var regex = new RegExp(regexString);
  var results = regex.exec(window.location.href);
  if (results == null) {
    return "";
  } else {
    return results[1];
  }
}

// Create new URL by given html page name and querystring, based on current URL
// path.
function buildURL(pageName, queryString) {
  var path = window.location.pathname;
  var url = "";

  url += window.location.protocol + "//" + window.location.host;
  if (path.lastIndexOf("/") > 0) {
    url += path.substring(0, path.lastIndexOf("/")) + "/" + pageName;
  } else {
    url += "/" + pageName;
  }
  url += ((queryString == "") ? "" : "?" + queryString);
  return url;
}

// Helper function for insertControl.
function generateControlHtml(configuration) {
  var objectAttributes = new Object();
  var params = new Array();
  var embedAttributes = new Object();
  var param;
  var html;

  function stringifyAttributes(attributeCollection) {
    var result = new String();
    for (var attributeName in attributeCollection) {
      result += ' ' + attributeName + '="' +
           attributeCollection[attributeName] + '"';
    }
    return result;
  }

  function applyAttribute(attributeCollection, name, value, defaultValue) {
    if (value === undefined)
      value = defaultValue;
    if (value !== null)
      attributeCollection[name] = value;
  }

  objectAttributes.classid="CLSID:E0A900DF-9611-4446-86BD-4B1D47E7DB2A";
  objectAttributes.codebase="http://www.google.com";
  applyAttribute(objectAttributes, "id", configuration.id, "ChromeFrame");
  applyAttribute(objectAttributes, "width", configuration.width, "500");
  applyAttribute(objectAttributes, "height", configuration.height, "500");

  // Attributes provided by the caller override any defaults.
  for (var attribute in configuration.objectAttributes) {
    if (configuration.objectAttributes[attribute] === null)
      delete objectAttributes[attribute];
    else
      objectAttributes[attribute] = configuration.objectAttributes[attribute];
  }

  embedAttributes.type = "application/chromeframe";

  // By default, embed id = object.id + "Plugin".  null id means omit id.
  if (embedAttributes.id === null)
    delete embedAttributes.id;
  else if (embedAttributes.id === undefined && objectAttributes.id !== null)
    embedAttributes.id = objectAttributes.id + "Plugin";

  // By default, embed name = object.id.  null name means omit name.
  if (embedAttributes.name === null)
    delete embedAttributes.name;
  else if (embedAttributes.name === undefined && objectAttributes.id !== null)
    embedAttributes.name = objectAttributes.id;

  applyAttribute(embedAttributes, "width", configuration.width, "500");
  applyAttribute(embedAttributes, "height", configuration.height, "500");
  applyAttribute(embedAttributes, "src", configuration.src, null);

  for (var attribute in configuration.embedAttributes) {
    if (configuration.embedAttributes[attribute] === null)
      delete embedAttributes[attribute];
    else
      embedAttributes[attribute] = configuration.embedAttributes[attribute];
  }

  if (embedAttributes.src !== undefined) {
    param = new Object();
    param.name = "src";
    param.value = embedAttributes.src;
    params.push(param);
  }

  // All event handlers are params and attributes of the embed object.
  for (var eventName in configuration.eventHandlers) {
    param = new Object();
    param.name = eventName;
    param.value = configuration.eventHandlers[eventName];
    params.push(param);
    embedAttributes[eventName] = configuration.eventHandlers[eventName];
  }

  html = "<object" + stringifyAttributes(objectAttributes) + ">\r\n";
  for (var i = 0; i < params.length; ++i) {
    html += "  <param" + stringifyAttributes(params[i]) + ">\r\n";
  }
  html += "  <embed" + stringifyAttributes(embedAttributes) + "></embed>\r\n"

  html += "</object>";

  return html;
}

// Write the text for the Chrome Frame ActiveX control into an element.
// This works around a "feature" of IE versions released between April, 2006
// and April, 2008 that required the user to click on an ActiveX control to
// "activate" it.  See http://msdn.microsoft.com/en-us/library/ms537508.aspx.
//
// |elementId| identifies the element in the current document into which the
// control markup will be inserted.  |configuration| is an Object used to
// configure the control as below.  Values shown are defaults, which may be
// overridden by supplying null values.
// {
//   "id": "ChromeFrame", // id of object tag, name of the embed tag, and
//                        // basis of id of the embed tag.
//   "width": "500", // width of both object and embed tags.
//   "height": "500", // height of both object and embed tags.
//   "src": "url", // src of embed tag and of param to object tag.
//   "eventHandlers": { // Applied to embed tag and params to object tag.
//     "onclose": "...",
//     "onload": "...",
//     "onloaderror": "..."
//   }
//   "objectAttributes": { // Custom attributes for the object tag. Any
//     "tabindex": "...",  // properties explicitly set to null will override
//     "border": "...",    // defaults.
//     "style": "..."
//   },
//   "embedAttributes": {        // Custom attributes for the embed tag;
//     "privileged_mode": "...", // similar to above.
//     "style": "..."
//   }
// }
function insertControl(elementId, configuration) {
  var e = document.getElementById(elementId);
  e.innerHTML = generateControlHtml(configuration);
}
