FROM ubuntu:trusty

# Run the command below to update the lib list.
# ldd ./blimp_engine_app | grep usr/lib | awk '{print $3}' | xargs -n1 \
#   dpkg-query -S | awk -F: '{print $1}' | sort | uniq
RUN apt-get update && \
  apt-get install -yq libdrm2 libfontconfig1 libfreetype6 libgraphite2-3 \
  libharfbuzz0b libnspr4 libnss3 libstdc++6

RUN apt-get update && apt-get install -yq stunnel4 wget

RUN mkdir /engine

RUN useradd -ms /bin/bash blimp_user

# The glob below expands to all files, but does not add directories
# recursively.
ADD * /engine/
ADD gen/third_party/blimp_fonts /engine/fonts
RUN mv /engine/chrome_sandbox /engine/chrome-sandbox
RUN chown -R blimp_user /engine

USER blimp_user
WORKDIR "/engine"

ENTRYPOINT ["/engine/start_engine.sh"]
