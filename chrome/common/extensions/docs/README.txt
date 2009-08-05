This directory contains the chromium extensions documentation, and the mechanism
by which they are generated.

--------------------------------------------------------------------------------
Building

Changes to the extension docs must be checked into source control. Any changes
to any input sources require the docs to be regenerated.

To build the extension docs, run the build.py script in the ./build directory.
This will regenerate the docs and report which, if any, files have changed
and need to be included in the changelist that changed the dependent files.

Note that the build.py script depends on test_shell to run, so in you must be
able to build test_shell to build extension_docs. The build.py script will
look in typically locations for the test_shell executable, but you may set
the path to test_shell explicitly with --test-shell-path.

--------------------------------------------------------------------------------
Design

I. Inputs

There are two sources of input:

1) The contents of ../api/extension_api.json
which contains the "IDL" of the the methods, events, and types of the api. This
file is linked as a resource into the chromium binary and then dynamically
bound to chrome.* objects that are exposed to extension contexts. This file
is fed into the api docs template. It contains both name, type information as
well as documentation contained in "description" properties.

2) The set of ./static/*.html documents. Each of these static html fragments is
inserted into a common template and rendered into ./*.html.

II. Processing

The processing of each document page is as follows:

For each given <page>:
1) A copy of ./page_shell.html is copied to ./<page>.html.
2) This page loads bootstrap.js which inspects the <page> name
3) ./template/api_template.html is loaded and inserted into the body of the page
4) If a ./static/<page>.html exists, it's content is inserted into the main
content column of the api_template.html
5) If the <page> matches an api "module" in extension_api.json, the api is then
fed through the api template within api_template.html
6) The result is writen ontop of the existing /<page>.html. If the new file
differs in content, it is reported as changed by the build.py script.
