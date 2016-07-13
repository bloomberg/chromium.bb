# This Dockerfile is used to build a filesystem environment containing
# binaries and required files for Blimp Engine test and tests for dependencies
# of Blimp Engine. It is built on the same base image that is used to run the
# Engine itself.
FROM base:latest

RUN mkdir -p /out/test/

# The glob below expands to all files, but does not add directories
# recursively.
# Test binaries assume src directory is 2 levels down. While this behavior can
# be changed with flags, the directory structure is set up to minimize
# potential problems with initial integration.
ADD * /out/test/

RUN chown -R blimp_user /out/test/

USER blimp_user