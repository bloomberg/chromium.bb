
// We ask the background page to get the extension API to test against. When it
// responds we start the test.
console.log("asking for api ...");
chrome.extension.sendRequest("getApi", function(apis) {
  console.log("got api response");
  var privilegedPaths = [];
  var unprivilegedPaths = [];
  apis.forEach(function(module) {
    var namespace = module.namespace;
    if (!module.unprivileged) {
      privilegedPaths.push(namespace);
      return;
    }

    ["functions", "events"].forEach(function(section) {
      if (typeof(module[section]) == "undefined")
        return;
       module[section].forEach(function(entry) {
        var path = namespace + "." + entry.name;
        if (entry.unprivileged) {
          unprivilegedPaths.push(path);
        } else {
          privilegedPaths.push(path);
        }
      });
    });

    if (module.properties) {
      for (var propName in module.properties) {
        var path = namespace + "." + propName;
        if (module.properties[propName].unprivileged) {
          unprivilegedPaths.push(path);
        } else {
          privilegedPaths.push(path);
        }
      }
    }
  });
  doTest(privilegedPaths, unprivilegedPaths);
});


// Tests whether missing properties of the chrome object correctly throw an
// error on access. The path is a namespace or function/property/event etc.
// within a namespace, and is dot-separated.
function testPath(path, expectError) {
  console.log("trying " + path);
  var parts = path.split('.');

  // Iterate over each component of the path, making sure all but the last part
  // is defined. The last part should either be defined or throw an error on
  // attempted access.
  var module = chrome;
  for (var i = 0; i < parts.length; i++) {
    if (i < parts.length - 1) {
      // Not the last component, so expect non-null / no exception.
      try {
        module = module[parts[i]];
      } catch (err) {
        console.log("testPath failed on subcomponent of " + path);
        return false;
      }
    } else {
      // This is the last component - we expect it to either be defined or
      // to throw an error on access.
      try {
        if (typeof(module[parts[i]]) == "undefined") {
          console.log(" fail (undefined and not throwing error): " +
                      path);
          return false;
        } else if (!expectError) {
          console.log(" ok (defined): " + path);
          return true;
        }
      } catch (err) {
        if (!expectError) {
          console.log(" fail (did not expect error): " + path);
          return false;
        }
        var str = err.toString();
        if (str.search("is not supported in content scripts") != -1) {
          console.log(" ok (correct error thrown): " + path);
          return true;
        } else {
          console.log(" fail (wrong error: '" + str + "')");
          return false;
        }
      }
    }
  }
  console.log(" fail (no error when we were expecting one): " + path);
  return false;
}

function displayResult(status) {
  var div = document.createElement("div");
  div.innerHTML = "<h1>" + status + "</h2>";
  document.body.appendChild(div);
}

function reportSuccess() {
  displayResult("pass");
  chrome.extension.sendRequest("pass");
}

function reportFailure() {
  displayResult("fail");
  // Let the "fail" show for a little while so you can see it when running
  // browser_tests in the debugger.
  setTimeout(function() {
    chrome.extension.sendRequest("fail");
  }, 1000);
}

// Runs over each string path in privilegedPaths and unprivilegedPaths, testing
// to ensure a proper error is thrown on access or the path is defined.
function doTest(privilegedPaths, unprivilegedPaths) {
  console.log("starting");

  if (!privilegedPaths || privilegedPaths.length < 1 || !unprivilegedPaths ||
      unprivilegedPaths.length < 1) {
    port.postMessage("fail");
    return;
  }

  var failures = [];
  var success = true;

  // Returns a function that will test a path and record any failures.
  function makeTestFunction(expectError) {
    return function(path) {
      if (!testPath(path, expectError)) {
        success = false;
        failures.push(path);
      }
    };
  }
  privilegedPaths.forEach(makeTestFunction(true));
  unprivilegedPaths.forEach(makeTestFunction(false));

  console.log(success ? "pass" : "fail");
  if (success) {
    reportSuccess();
  } else {
    console.log("failures on:\n" + failures.join("\n"));
    reportFailure();
  }
}

