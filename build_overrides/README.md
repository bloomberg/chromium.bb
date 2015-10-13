# Build overrides in GN

This directory is used to allow different products to customize settings
for repos that are DEPS'ed in or shared.

For example: V8 could be built on its own (in a "standalone" configuration),
and it could be built as part of Chromium. V8 might define a top-level
target, //v8:d8 (a simple executable), that should only be built in the
standalone configuration. To figure out whether or not it should be
in a standalone configuration, v8 can create a file, build_overrides/v8.gni,
that contains a variable, `build_standalone_d8 = true`.
and import it (as import("//build_overrides/v8.gni") from its top-level
BUILD.gn file.

Chromium, on the other hand, might not need to build d8, and so it would
create its own build_overrides/v8.gni file, and in it set 
`build_standalone_d8 = false`. 

The two files should define the same set of variables, but the values can
vary as appropriate to the needs of the two different builds.
