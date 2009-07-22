/**
 * @fileoverview This file is the controller for generating one api reference
 * page of the extension documentation.
 *
 * It expects:
 *
 * - To be called from a "shell" page whose "base" name matches an api module
 *   name. For instance ../bookmarks.html -> chrome.bookmarks.
 * 
 * - To have available via XHR (relative path):
 *   1) API_TEMPLATE which is the main template for the api pages.
 *   2) A file located at SCHEMA_PATH + |apiName| + SCHEMA_EXTENSION
 *      which is shared with the extension system and defines the methods and 
 *      events contained in one api.
 *   3) An |apiName| + OVERVIEW_EXTENSION file which contains static authored
 *      content that is inserted into the "overview" slot in the API_TEMPLATE.
 *
 * The "shell" page may have a renderering already contained within it so that
 * the docs can be indexed.
 *
 * TODO(rafaelw): XHR support for IE.
 * TODO(rafaelw): JSON support for non-chrome 3.x clients.
 */
 
var API_TEMPLATE = "../template/api_template.html";
var SCHEMA_PATH = "../../api/";
var SCHEMA_EXTENSION = ".json";
var OVERVIEW_EXTENSION = "_overview.html";
var REQUEST_TIMEOUT = 2000;

Array.prototype.each = function(f) {
  for (var i = 0; i < this.length; i++) {
    f(this[i], i);
  }
}

window.onload = function() {
  // Determine api module being rendered. Expect ".../<apiName>.html"
  var pathParts = document.location.href.split(/\/|\./);
  var apiName = pathParts[pathParts.length - 2];
  var apiOverviewName = apiName + OVERVIEW_EXTENSION;
  var apiSchemaName = SCHEMA_PATH + apiName + SCHEMA_EXTENSION;
  
  // Fetch the api template and insert into the <body>.
  fetchContent(API_TEMPLATE, function(templateContent) {
    document.getElementsByTagName("body")[0].innerHTML = templateContent;

    // Fetch the overview and insert into the "overview" <div>.
    fetchContent(apiOverviewName, function(overviewContent) {
      document.getElementById("overview").innerHTML = overviewContent;

      // Now the page is composed with the authored content, we fetch the schema
      // and populate the templates.
      fetchContent(apiSchemaName, renderTemplate);	
    });
  });	
}

/**
 * Fetches |url| and returns it's text contents from the xhr.responseText in
 * onSuccess(content)
 */
function fetchContent(url, onSuccess) {
  var xhr = new XMLHttpRequest();
  var abortTimerId = window.setTimeout(function() {
    xhr.abort();
    console.log("XHR Timed out");
  }, REQUEST_TIMEOUT);

  function handleError(error) {
    window.clearTimeout(abortTimerId);
    console.error("XHR Failed: " + error);
  }

  try {
    xhr.onreadystatechange = function(){
      if (xhr.readyState == 4) {
        if (xhr.responseText) {
          window.clearTimeout(abortTimerId);
          onSuccess(xhr.responseText);
        } else {
          handleError("responseText empty.");
        }
      }
    }
    
    xhr.onerror = handleError;
    
    xhr.open("GET", url, true);
    xhr.send(null);
  } catch(e) {
    console.log("ex: " + e);
    console.error("exception: " + e);
    handleError();
  }
}

/**
 * Parses the content in |module| to json, adds any additional required values,
 * renders to html via JSTemplate, and unhides the <body>.
 * This uses the root <html> element (the entire document) as the template.
 */
function renderTemplate(module) {
  var apiDefinition = JSON.parse(module);
  preprocessApi(apiDefinition);

  // Render to template
  var input = new JsEvalContext(apiDefinition);
  var output = document.getElementsByTagName("html")[0];
  jstProcess(input, output);
  
  // Show.
  document.getElementsByTagName("body")[0].className = "";
}

/**
 * Augment the |schema| with additional values that are required by the
 * template.
 */
function preprocessApi(schema) {
  schema.functions.each(function(f) {
    f.fullName = schema.namespace + "." + f.name;
    if (f.callbackParameters) {
      f.callbackSignature = generateSignatureString(f.callbackParameters);
    }
  });
    
  schema.events.each(function(e) {
    e.callSignature = generateSignatureString(e.parameters);
  });
}

/** 
 * Generates a simple string representation of the signature of a function
 * whose |parameters| are json schemas.
 */
function generateSignatureString(parameters) {
  var retval = [];
  parameters.each(function(param, i) {
    retval.push(param.type + " " + (param.type ? param.type : param["$ref"]));
  });
  
  return retval.join(", ");	
}
