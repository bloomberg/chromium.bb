# Running the engine in a Docker container

For local development and testing, you can run the engine in a Docker
container.

The steps are:

1. Bundle the engine and its dependencies.

1. Build a Docker image.

1. Create a Docker container.


## About Docker

To get a high-level overview of Docker, please read [Understand the
architecture](https://docs.docker.com/introduction/understanding-docker/).
Optional reading includes reference guides for
[`docker run`](https://docs.docker.com/reference/run/) and
[Dockerfile](https://docs.docker.com/reference/builder/).


### Installation

For Googlers running Goobuntu wanting to install Docker, see
[go/installdocker](https://goto.google.com/installdocker). For other
contributors using Ubuntu, see [official Docker
installation instructions](https://docs.docker.com/installation/ubuntulinux/).


## Bundle Engine

The `blimp/engine:blimp_engine_bundle` build target will bundle the engine and
its dependencies into a tarfile, which can be used to build a Docker image.
This target is always built as part of the top-level `blimp/blimp` meta-target.

### Update Engine Dependencies

`blimp/engine/engine-manifest.txt` is a list of the engine's runtime
dependencies. From time to time, this list may need to be updated. Use
`blimp/tools/generate-engine-manifest.py` to (re)generate the manifest:

```bash
./blimp/tools/generate-engine-manifest.py \
    --build-dir out-linux/Debug \
    --target //blimp/engine:blimp_engine \
    --output blimp/engine/engine-manifest.txt
```

Be sure to review the generated manifest and remove any false runtime
dependencies.

## Build Docker Image

Using the tarfile you can create a Docker image:

```bash
docker build -t blimp_engine - < ./out-linux/Debug/blimp_engine_bundle.tar
```

## Create Docker Container

From the Docker image you can create a Docker container (i.e. run the engine):

```bash
docker run blimp_engine
```

You can also pass additional flags:

```bash
docker run blimp_engine --with-my-flags
```

See the [blimp engine `Dockerfile`](../engine/Dockerfile) to find out what flags
are passed by default.

### Mapping Volumes into the Docker Container

If you need to map a directory into the Docker container (eg. for necessary
files):

```bash
docker run -v /path/to/srcdir:/path/to/docker/destdir blimp_engine
```

NB: The permissions of the directory and the files outside of the Docker
container will be carried over into the container.
