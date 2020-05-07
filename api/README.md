# Build API

Welcome to the Build API.

## Getting Started

### Overview

The Build API is a CLI-only, proto based API to execute build steps.
It was created to provide a stable interface for the CI builders.
The proto files (in [chromite/infra/proto](#chromite/infra/proto/)) define the
services/RPCs provided by the API.
The modules in
[controller/](https://chromium.googlesource.com/chromiumos/chromite/+/refs/heads/master/api/controller/)
are the entry points for the RPCs defined in the proto files.
The Build API is invoked via the `build_api` script, which takes 4 arguments;
the name of the endpoint being called (e.g. chromite.api.SdkService/Create),
and the input, output, and optional config protos, which are provided as paths
to files containing the respective JSON or protobuf-binary encoded messages.

### Calling An Endpoint

To manually call an endpoint, e.g. for testing, the
[gen_call_scripts](https://chromium.googlesource.com/chromiumos/chromite/+/refs/heads/master/api/contrib/README.md#gen_call_scripts_call_templates_and-call_scripts)
process is recommended, it makes calling a specific endpoint much
simpler. Please also contribute new example input files when you
add new endpoints!

The overall process is simple whether you want to do it manually
or in a script, though. You'll need to build out an instance of
the request message and write it to a file, and then just call
the `build_api` script.

The only tricky part is getting the compiled protobuf. If you're
working in recipes or in chromite the problem has already been
addressed. Otherwise, you'll need to figure out a process that
works for you depending on your language and purpose. For immediate,
local work, compiling the proto with protoc should be relatively
straightforward, but for production services consulting the CrOS CI
team may be worthwhile.

## API Developer Guide

This section contains information for developers contributing to the Build API.

### Special Build API Proto Behavior

The Build API has some special, automatic functionality that triggers for a few
specific proto messages/fields. These are meant to facilitate some specific
aspects of the Build API and reduce boilerplate code.

#### Service & Method Options

The service and method options extensions in `chromite/api/build_api.proto`
define some key information about the implementation of the endpoint.

The first are the `module` and `implementation_name` fields, in the service and
method options respectively, that tell the Build API how to call the endpoint.
The service's `module` option is required, and defines the name of the
[controller](#controller) module where the service's RPCs are implemented.
The method's `implementation_name` field is optional, and defines the name of
the function in the service's controller module for the RPC.
When the `implementation_name` is not given, the API expects the function to
have the same name as the RPC in the proto.

The two options extensions also define the `service_chroot_assert` and
`method_chroot_assert` fields, respectively.
These two fields allow defining whether the endpoint must run `INSIDE` or
`OUTSIDE` the chroot, or if either is fine when not set.
The service's option is the default for all of its RPCs, and the RPC's method
option overrides it when set.

#### `INSIDE` chroot endpoints

At first the `INSIDE` behavior can be somewhat confusing, but is designed to
reduce boilerplate by automating chroot interactions.
When writing an endpoint that needs to run inside the chroot, setting the
chroot assertion to `INSIDE` is not required, but is strongly recommended for
the sake of simplifying the implementation and standardizing the chroot
interactions.

All endpoints that do use the `INSIDE` functionality must have a
`chromiumos.Chroot` field, but are otherwise free to use whatever it needs.
The Build API parses the `chromiumos.Chroot` field, removes it from the input,
then executes the endpoint inside the chroot.
The endpoint's implementation can be written as if it is always inside the
chroot, because while Build API invocations are always made from outside the
chroot, it ensures the implementation does not even get imported until after it
has entered the chroot.

#### `INSIDE` Chroot `Path` Utilities

The automatic chroot handling means inserting and extracting artifacts is not
possible manually.
This gap has been filled by the `Path`, `ResultPath`, and `SyncedDir` messages,
defined in `chromiumos/common.proto`, that allows the implementations to always
behave as if they are working with local files without considering chroot
pathing implications.

The `Path` message tells the Build API a path, and whether the path is inside
or outside of the chroot.
A `Path` message in a request can be used to inject a file or folder into the
chroot for the endpoint to use.
Before entering the chroot, the Build API copies the path into a temporary
directory inside the chroot, and changes the path in the request sent to the
inside-chroot invocation of the endpoint to point to that inside chroot path.
For example, if you need `/working/directory/image.bin` for an endpoint inside
the chroot, the Build API will create a `/path/to/chroot/tmp/rand-tmp-dir/`,
copy in `image.bin`, and then the implementation running inside the chroot will
be given `/tmp/rand-tmp-dir/image.bin`.

The `ResultPath` message provides a similar functionality for extracting paths
from the chroot.
The`ResultPath` message itself must be defined in the request, and is analogous
to passing a function an output directory.
To use, simply set the paths of the response `Path` messages to the files or
directories that need to be extracted from the chroot.
After the endpoint execution completes, all `Path` messages in the response are
copied into the given result path, and the paths in the response are updated to
reflect their final location.
Worth noting, the implementation does not require gathering artifacts to a
specific location inside the chroot, the Build API will handle gathering the
files into the ResultPath outside the chroot.

The `SyncedDir` message in a request provides a blanket, bidirectional sync of
a directory.
Any files present in the specified directory are copied into a temp directory
in the chroot before it is executed, then after it finishes the source directory
is emptied, and all files in the chroot directory are copied out to the source
directory.
This message can be useful for situations where the directory structure or
contents is not necessarily important, for example, setting a process' log
directory to the `SyncedDir` path allows extracting all the log files.

## Directory Reference

### chromite/infra/proto/

**Make sure you've consulted the Build and CI teams when considering making
breaking changes that affect the Build API.**

This directory is a separate repo that contains all of the raw .proto files. You
will find message, service, and method definitions, and their configurations.
You will need to commit and upload the proto changes separately from the
chromite changes.

* chromite/api/ contains the Build API services.
  * Except chromite/api/build_api.proto, which contains service and method
    option definitions.
  * And build_api_test.proto which is used only for testing the Build API itself.
* chromiumos/ generally contains more shareable proto.
  * chromiumos/common.proto contains well shared messages.
  * chromiumos/metrics.proto contains message declarations related to build api
    event monitoring.
* test_platform/ contains the APIs of components of the Test Platform recipe.
  * test_platform/request.proto and test_platform/response.proto contain the API
    of the overall recipe.
* device/ contains the proto for hardware related configuration.

When making changes to the proto, you must:

* Change the proto.
  1. Make your changes.
  1. Run `chromite/infra/proto/generate.sh`.
  1. Commit those changes as a single CL.
* Update the chromite proto.
  * Run `chromite/api/compile_build_api_proto`.
  * When no breaking changes are made (should be most of them)
    * Create a CL with just the generated proto to submit with the raw proto CL.
    * Submit the proto CLs together.
    * The implementation may be submitted after the proto CLs.
  * When breaking changes are made (should be very rare)
    * **Make sure you've consulted the Build and CI teams first.**
    * Submit the proto changes along with the implementation.
    * May be done as a single CL or as a stack of CLs with `Cq-Depend`.

At time of writing, the PCQ does not support `Cq-Depend:` between the infra/proto
and chromite repos, so it must be handled manually.

This repo was and will be pinned to a specific revision in the manifest files
when we get closer to completing work on the Build API. For the speed we're
moving at now, though, having that revision pinned and updating the manifest has
caused far more problems than its solving.

When we do go back to the pinned revision:

1. Commit and land your proto changes.
1. Update manifest/full.xml with the new revision.
1. Update manifest-internal/external-full.xml with the new revision.
1. Run chromite/api/compile_build_api_proto.
1. Upload the changes from the previous three steps and add CQ-DEPEND between
   the CLs.

### gen/
The generated protobuf messages.

**Do not edit files in this package directly!**

The proto can be compiled using the `compile_build_api_proto` script in the api
directory. The protoc version is locked and fetched from CIPD to ensure
compatibility with the client library in `third_party/`.

### controller/

This directory contains the entry point for all of the implemented services. The
protobuf service module option (defined in build_api.proto) references the
module in this package. The functions in this package should all operate as one
would expect a controller to operate in an MVC application - translating the
request into the internal representation that's passed along to the relevant
service(s), then translates their output to a specified response format.

### contrib/

This directory contains scripts that may not be 100% supported yet.
See [`contrib/README.md`](https://chromium.googlesource.com/chromiumos/chromite/+/refs/heads/master/api/contrib/README.md)
for information about the scripts.
