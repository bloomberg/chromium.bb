# Build API

Welcome to the Build API.

### chromite/infra/proto/
This directory is a separate repo that contains all of the raw .proto files. You
will find message, service, and method definitions, and their configurations.
You will need to commit and upload the proto changes separately from the
chromite changes.

* chromite/api/ contains the Build API services.
  * Except chromite/api/build_api.proto, which contains service and method
  * option definitions.
* chromiumos/ generally contains more sharable proto.
  * chromiumos/common.proto contains well shared messages.
  * chromiumos/metrics.proto contains message declarations related to build api
    event monitoring.
* test_platform/ contains the APIs of components of the Test Platform recipe.
  * test_platform/request.proto and test_platform/response.proto contain the API
  * of the overall recipe.

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
directory. The protoc call is executed inside the chroot to ensure a standard
protoc version is used.

### controller/

This directory contains the entry point for all of the implemented services. The
protobuf service module option (defined in build_api.proto) references the
module in this package. The functions in this package should all operate as one
would expect a controller to operate in an MVC application - translating the
request into the internal representation that's passed along to the relevant
service(s), then translates their output to a specified response format.
