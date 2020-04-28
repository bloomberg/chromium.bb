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
