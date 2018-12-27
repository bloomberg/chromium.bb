module.exports = function(grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON("package.json"),

    clean: ["out/", "out-node/"],

    run: {
      "generate-listing": {
        args: [
          "node-run",
          "--generate-listing",
          "out/cts/listing.json",
        ],
      },
      "node-run": {
        args: [
          "node-run",
          "--run",
        ],
      },
    },

    "http-server": {
      "out/": {
        root: ".",
        port: 8080,
        host: "127.0.0.1",
        cache: 5,
      }
    },

    ts: {
      "out-node/": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
        options: {
          additionalFlags: "--noEmit false --outDir out-node/ --module commonjs",
        }
      },
      "out/": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
        options: {
          additionalFlags: "--noEmit false --outDir out/",
        }
      },
    },

    tslint: {
      options: {
        configuration: "tslint.json",
      },
      files: {
        src: [ "src/**/*.ts" ],
      },
    },
  });

  grunt.loadNpmTasks("grunt-contrib-clean");
  grunt.loadNpmTasks("grunt-http-server");
  grunt.loadNpmTasks("grunt-run");
  grunt.loadNpmTasks("grunt-ts");
  grunt.loadNpmTasks("grunt-tslint");

  const publishedTasks = [];
  function publishTask(name, desc, deps) {
    grunt.registerTask(name, deps);
    publishedTasks.push({name, desc});
  }

  publishTask("build", "Build out/", [
    "node-build",
    "ts:out/",
    "run:generate-listing",
  ]);
  publishTask("serve", "Serve out/ on 127.0.0.1:8080", [
    "http-server:out/",
  ]);

  publishTask("node-build", "Build out-node/", [
    "ts:out-node/",
  ]);
  publishTask("node-run", "Run out-node/", [
    "node-build",
    "run:node-run",
  ]);
  publishedTasks.push({name: "clean", desc: "Clean out/, out-node/"});

  grunt.registerTask("default", "", () => {
    console.log("Available tasks (see grunt --help for more):");
    for (const {name, desc} of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });
};
