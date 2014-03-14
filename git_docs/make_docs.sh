#!/bin/bash -e
shopt -s nullglob

cd $(dirname "$0")

# Script which takes all the asciidoc git-*.txt files in this directory, renders
# them to html + manpage format using git 1.9's doc toolchain, then puts
# them in depot_tools to be committed.

ensure_in_path() {
  local CMD=$1
  local PTH=$(which "$CMD")
  if [[ ! $PTH ]]
  then
    echo Must have "$CMD" on your PATH!
    exit 1
  else
    echo Using \'$PTH\' for ${CMD}.
  fi
}

ensure_in_path xmlto
ensure_in_path asciidoc

DFLT_CATALOG_PATH="/usr/local/etc/xml/catalog"
if [[ ! $XML_CATALOG_FILES && -f "$DFLT_CATALOG_PATH" ]]
then
  # Default if you install doctools with homebrew on mac
  export XML_CATALOG_FILES="$DFLT_CATALOG_PATH"
  echo Using \'$DFLT_CATALOG_PATH\' for \$XML_CATALOG_FILES.
fi

# We pull git to get its documentation toolchain
BRANCH=v1.9.0
GITHASH=5f95c9f850b19b368c43ae399cc831b17a26a5ac
if [[ ! -d git || $(git -C git rev-parse HEAD) != $GITHASH ]]
then
  echo Cloning git
  rm -rf git
  git clone --single-branch --branch $BRANCH --depth 1 \
    https://kernel.googlesource.com/pub/scm/git/git.git  2> /dev/null

  # Replace the 'source' and 'package' strings.
  ed git/Documentation/asciidoc.conf <<EOF
  H
  81
  s/Git/depot_tools
  +2
  s/Git Manual/Chromium depot_tools Manual
  wq
EOF
fi
echo Git up to date at $GITHASH \($BRANCH\)

HTML_TARGETS=()
MAN_TARGETS=()
for x in *.txt
do
  TO="git/Documentation/$x"
  if [[ ! -f "$TO" ]] || ! cmp --silent "$x" "$TO"
  then
    echo \'$x\' differs
    cp $x "$TO"
  fi
  # Exclude files beginning with _ from the target list. This is useful to have
  # includable snippet files.
  if [[ ${x:0:1} != _ ]]
  then
    HTML_TARGETS+=("${x%%.txt}.html")
    MAN_TARGETS+=("${x%%.txt}.1")
  fi
done

VER="v$(git rev-parse --short HEAD)"
if [[ ! -f git/version ]] || ! cmp --silent git/version <(echo "$VER")
then
  echo Version changed, cleaning.
  echo "$VER" > git/version
  (cd git/Documentation && make clean)
fi

# This export is so that asciidoc sys snippets which invoke git run relative to
# depot_tools instead of the git clone.
(
  export GIT_DIR="$(git rev-parse --git-dir)" &&
  cd git/Documentation &&
  make -j"$[${#MAN_TARGETS} + ${#HTML_TARGETS}]" "${MAN_TARGETS[@]}" "${HTML_TARGETS[@]}"
)

mkdir htmlout 2> /dev/null || true
for x in "${HTML_TARGETS[@]}"
do
  echo Copying htmlout/$x
  cp "git/Documentation/$x" htmlout
done

for x in "${MAN_TARGETS[@]}"
do
  echo Copying ../man1/$x
  cp "git/Documentation/$x" ../man1
done