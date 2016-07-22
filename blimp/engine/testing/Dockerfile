# This Dockerfile is used to build a filesystem environment containing
# binaries and required files for Blimp Engine test and tests for dependencies
# of Blimp Engine. It is built on the same base image that is used to run the
# Engine itself.
FROM base:latest

RUN mkdir -p /blimp/

RUN useradd -ms /bin/bash blimp_user

# Put all the Blimp related files in /blimp so they are kept separate from
# the OS files. Using '.' instead of '*' ensures directory structure is
# maintained since ADD only copies the contents of directories.
ADD . /blimp/

RUN chown -R blimp_user /blimp

USER blimp_user