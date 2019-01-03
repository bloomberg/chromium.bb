module.exports = function(grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON("package.json"),

    clean: ["out/"],

    run: {
      "generate-listing": {
        cmd: "npx",
        args: [
          "ts-node",
          "--transpile-only",
          '-O{"module":"commonjs"}',
          "node-run",
          "src/cts",
          "--generate-listing=out/cts/listing.json",
        ],
      },
      "node-run": {
        cmd: "npx",
        args: [
          "ts-node",
          "--transpile-only",
          '-O{"module":"commonjs"}',
          "node-run",
          "src/cts",
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
    "ts:out/",
    "run:generate-listing",
  ]);
  publishTask("serve", "Serve out/ on 127.0.0.1:8080", [
    "http-server:out/",
  ]);

  publishTask("node-run", "Run in Node", [
    "run:node-run",
  ]);
  publishedTasks.push({name: "clean", desc: "Clean out/"});

  grunt.registerTask("default", "", () => {
    console.log("Available tasks (see grunt --help for more):");
    for (const {name, desc} of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });
};
