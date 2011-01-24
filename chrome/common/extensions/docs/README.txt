This directory contains the chromium extensions documentation, and the mechanism
by which they are generated.

--------------------------------------------------------------------------------
Contributing To The Extension Docs:

[When making changes, open the relevant /<page>.html in chrome via the file:
scheme. If you do, you can refresh to instantly see any changes you make].

*I want to document methods, events or parameters in the api itself:
=>Edit ../api/extension_api.json. Usually you can just add or edit the
"description" property. This will appear automatically in the corresponding doc
page at ./<page>.html (where <page> is the name of the apimodule ("tabs", etc..)
that contains the change you are making.

*I want to edit static content for an API reference module:
=>Look in /static/<page>.html (for your module). If the file exists, edit it,
check you changes by viewing /<page>.html. If the file doesn't exist, add it,
and make a copy of /template/page_shell.html and copy it to /<page>.html.

*I want to edit or add a purely static page:
=>Follow the same steps for editing static content for an API page.

IN ALL CASES. When you have finished, run build/build.bat (on windows) or
build/build.py (on mac/linux). This may generate new files or changes to the
/*.html pages. Include any new or changed files in the changelist you create.

--------------------------------------------------------------------------------
Building

Changes to the extension docs must be checked into source control. Any changes
to any input sources require the docs to be regenerated.

To build the extension docs, run the build.py script in the ./build directory.
This will regenerate the docs and report which, if any, files have changed
and need to be included in the changelist that changed the dependent files.

Note that the build.py script depends on DumpRenderTree to run, so you must be
able to build DumpRenderTree to build extension_docs. The build.py script will
look in typical locations for the DumpRenderTree executable, but you may set
the path to DumpRenderTree explicitly with --dump-render-tree-path.

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
4) If a ./static/<page>.html exists, its content is inserted into the main
content column of the api_template.html
5) If the <page> matches an api "module" in extension_api.json, the api is then
fed through the api template within api_template.html
6) The result is written on top of the existing /<page>.html. If the new file
differs in content, it is reported as changed by the build.py script.
